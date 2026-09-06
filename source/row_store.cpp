#include "tdb/row_store.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace std;

namespace
{
constexpr uint32_t row_page_magic = 0x53574f52; // "ROWS"
constexpr uint32_t overflow_page_magic = 0x46564f52; // "ROVF"
constexpr uint16_t occupied_flag = 1;
constexpr uint16_t overflow_flag = 2;
constexpr size_t header_size = 20;
constexpr size_t slot_size = 8;
constexpr size_t overflow_header_size = 16;
constexpr size_t overflow_capacity = pager::page_size - overflow_header_size;
// A spilled row keeps a fixed-size stub in the slotted page: the first overflow
// page number followed by the row's total length.
constexpr size_t overflow_stub_size = 12;
// The largest row that still fits in an otherwise empty slotted page. Anything
// larger spills, so rows at or below this size are stored exactly as before.
constexpr size_t maximum_inline_size = pager::page_size - header_size - slot_size;
constexpr uint64_t slot_mask = 0x0fff;
constexpr uint64_t generation_mask = 0xffff;
constexpr unsigned slot_shift = 16;
constexpr unsigned page_shift = 28;
// Keep the high bit clear so every row_id is directly representable by the
// B+tree's signed 64-bit value type.
constexpr uint64_t maximum_page = (uint64_t{1} << (63 - page_shift)) - 1;

struct row_page_header
{
    uint16_t slot_count;
    uint16_t live_count;
    uint16_t free_start;
    uint16_t free_end;
    uint64_t next_page;
};

struct row_slot
{
    uint16_t offset;
    uint16_t length;
    uint16_t generation;
    uint16_t flags;
};

struct overflow_page_header
{
    uint32_t byte_count;
    uint64_t next_page;
};

template<typename T>
T load(const pager::page& page, size_t offset)
{
    T result{};
    memcpy(&result, page.data() + offset, sizeof(result));
    return result;
}

template<typename T>
void store(pager::page& page, size_t offset, T value)
{
    memcpy(page.data() + offset, &value, sizeof(value));
}

row_page_header read_header(const pager::page& page)
{
    if (load<uint32_t>(page, 0) != row_page_magic)
        throw runtime_error("Invalid row-store page.");
    row_page_header result{
        load<uint16_t>(page, 4), load<uint16_t>(page, 6),
        load<uint16_t>(page, 8), load<uint16_t>(page, 10),
        load<uint64_t>(page, 12)};
    if (result.free_start != header_size + result.slot_count * slot_size ||
        result.free_start > result.free_end || result.free_end > pager::page_size ||
        result.live_count > result.slot_count)
        throw runtime_error("Corrupt row-store page header.");
    return result;
}

void write_header(pager::page& page, const row_page_header& header)
{
    store<uint32_t>(page, 0, row_page_magic);
    store<uint16_t>(page, 4, header.slot_count);
    store<uint16_t>(page, 6, header.live_count);
    store<uint16_t>(page, 8, header.free_start);
    store<uint16_t>(page, 10, header.free_end);
    store<uint64_t>(page, 12, header.next_page);
}

row_slot read_slot(const pager::page& page, uint16_t index)
{
    const size_t offset = header_size + static_cast<size_t>(index) * slot_size;
    return {load<uint16_t>(page, offset), load<uint16_t>(page, offset + 2),
            load<uint16_t>(page, offset + 4), load<uint16_t>(page, offset + 6)};
}

void write_slot(pager::page& page, uint16_t index, const row_slot& slot)
{
    const size_t offset = header_size + static_cast<size_t>(index) * slot_size;
    store<uint16_t>(page, offset, slot.offset);
    store<uint16_t>(page, offset + 2, slot.length);
    store<uint16_t>(page, offset + 4, slot.generation);
    store<uint16_t>(page, offset + 6, slot.flags);
}

void validate_slots(const pager::page& page, const row_page_header& header)
{
    uint16_t live = 0;
    for (uint16_t i = 0; i < header.slot_count; ++i)
    {
        const row_slot slot = read_slot(page, i);
        if ((slot.flags & occupied_flag) == 0) continue;
        ++live;
        if (slot.generation == 0 || slot.offset < header.free_end ||
            static_cast<size_t>(slot.offset) + slot.length > pager::page_size)
            throw runtime_error("Corrupt row-store slot.");
    }
    if (live != header.live_count)
        throw runtime_error("Corrupt row-store live count.");
}

size_t live_bytes(const pager::page& page, const row_page_header& header)
{
    size_t result = 0;
    for (uint16_t i = 0; i < header.slot_count; ++i)
    {
        const row_slot slot = read_slot(page, i);
        if ((slot.flags & occupied_flag) != 0) result += slot.length;
    }
    return result;
}

void compact(pager::page& page, row_page_header& header)
{
    validate_slots(page, header);
    pager::page packed{};
    uint16_t end = pager::page_size;
    for (uint16_t i = 0; i < header.slot_count; ++i)
    {
        row_slot slot = read_slot(page, i);
        if ((slot.flags & occupied_flag) != 0)
        {
            end = static_cast<uint16_t>(end - slot.length);
            memcpy(packed.data() + end, page.data() + slot.offset, slot.length);
            slot.offset = end;
        }
        write_slot(packed, i, slot);
    }
    header.free_end = end;
    write_header(packed, header);
    page = packed;
}

overflow_page_header read_overflow_header(const pager::page& page)
{
    if (load<uint32_t>(page, 0) != overflow_page_magic)
        throw runtime_error("Invalid row-store overflow page.");
    overflow_page_header result{load<uint32_t>(page, 4), load<uint64_t>(page, 8)};
    if (result.byte_count == 0 || result.byte_count > overflow_capacity)
        throw runtime_error("Corrupt row-store overflow page.");
    return result;
}

void write_overflow_header(pager::page& page, const overflow_page_header& header)
{
    store<uint32_t>(page, 0, overflow_page_magic);
    store<uint32_t>(page, 4, header.byte_count);
    store<uint64_t>(page, 8, header.next_page);
}

// Spills a row across a chain of overflow pages and returns the first page. The
// page numbers are reserved up front so each page can record its successor.
uint64_t write_overflow_chain(pager::transaction& tx, span<const uint8_t> bytes)
{
    const size_t count = (bytes.size() + overflow_capacity - 1) / overflow_capacity;
    if (count == 0) throw logic_error("An empty row never spills.");
    vector<uint64_t> pages;
    pages.reserve(count);
    for (size_t i = 0; i < count; ++i) pages.push_back(tx.allocate());
    for (size_t i = 0; i < count; ++i)
    {
        const size_t begin = i * overflow_capacity;
        const size_t length = (min)(overflow_capacity, bytes.size() - begin);
        pager::page page{};
        write_overflow_header(page, {static_cast<uint32_t>(length),
                                     i + 1 < count ? pages[i + 1] : uint64_t{0}});
        memcpy(page.data() + overflow_header_size, bytes.data() + begin, length);
        tx.write(pages[i]) = page;
    }
    return pages.front();
}

template<typename ReadPage>
vector<uint8_t> read_overflow_chain(ReadPage&& read_page, uint64_t first_page,
                                    uint32_t total_length)
{
    vector<uint8_t> result;
    result.reserve(total_length);
    uint64_t page_number = first_page;
    while (page_number != 0)
    {
        const pager::page& page = read_page(page_number);
        const overflow_page_header header = read_overflow_header(page);
        if (result.size() + header.byte_count > total_length)
            throw runtime_error("Row-store overflow chain is too long.");
        result.insert(result.end(), page.begin() + overflow_header_size,
                      page.begin() + overflow_header_size + header.byte_count);
        page_number = header.next_page;
    }
    if (result.size() != total_length)
        throw runtime_error("Row-store overflow chain is truncated.");
    return result;
}

// Reads the {first overflow page, total length} stub a spilled row leaves behind.
pair<uint64_t, uint32_t> read_overflow_stub(const pager::page& page, const row_slot& slot)
{
    if (slot.length != overflow_stub_size)
        throw runtime_error("Corrupt row-store overflow stub.");
    return {load<uint64_t>(page, slot.offset),
            load<uint32_t>(page, slot.offset + sizeof(uint64_t))};
}

row_id make_id(uint64_t page_number, uint16_t slot, uint16_t generation)
{
    if (page_number > maximum_page || slot > slot_mask || generation == 0)
        throw runtime_error("Row-store identifier space is exhausted.");
    return (page_number << page_shift) |
           (static_cast<uint64_t>(slot) << slot_shift) | generation;
}

uint64_t id_page(row_id id) { return id >> page_shift; }
uint16_t id_slot(row_id id) { return static_cast<uint16_t>((id >> slot_shift) & slot_mask); }
uint16_t id_generation(row_id id) { return static_cast<uint16_t>(id & generation_mask); }

template<typename ReadPage>
optional<vector<uint8_t>> get_row(ReadPage&& read_page, row_id id)
{
    const uint64_t page_number = id_page(id);
    if (page_number == 0 || id_generation(id) == 0) return nullopt;
    const pager::page& page = read_page(page_number);
    const row_page_header header = read_header(page);
    validate_slots(page, header);
    const uint16_t index = id_slot(id);
    if (index >= header.slot_count) return nullopt;
    const row_slot slot = read_slot(page, index);
    if ((slot.flags & occupied_flag) == 0 || slot.generation != id_generation(id))
        return nullopt;
    if ((slot.flags & overflow_flag) != 0)
    {
        const auto [first_page, total_length] = read_overflow_stub(page, slot);
        return read_overflow_chain(read_page, first_page, total_length);
    }
    return vector<uint8_t>(page.begin() + slot.offset,
                           page.begin() + slot.offset + slot.length);
}
}

row_id row_store::insert(b_tree_write_transaction& transaction,
                         span<const uint8_t> bytes) const
{
    transaction.require_active();
    if (bytes.size() > (numeric_limits<uint32_t>::max)())
        throw length_error("Row exceeds the 4 GiB row-store limit.");

    pager::transaction& tx = transaction._transaction;

    // Rows too large for a slotted page spill into a chain of overflow pages and
    // leave a fixed-size stub behind. From here down the stub is stored exactly
    // like any other small row, so slot allocation and compaction are unchanged.
    array<uint8_t, overflow_stub_size> stub{};
    span<const uint8_t> payload = bytes;
    const bool spilled = bytes.size() > maximum_inline_size;
    if (spilled)
    {
        const uint64_t first_page = write_overflow_chain(tx, bytes);
        const uint32_t total_length = static_cast<uint32_t>(bytes.size());
        memcpy(stub.data(), &first_page, sizeof(first_page));
        memcpy(stub.data() + sizeof(first_page), &total_length, sizeof(total_length));
        payload = stub;
    }
    const uint16_t new_flags =
        spilled ? static_cast<uint16_t>(occupied_flag | overflow_flag) : occupied_flag;

    uint64_t page_number = tx.row_page();
    while (page_number != 0)
    {
        pager::page candidate = tx.read(page_number);
        row_page_header header = read_header(candidate);
        validate_slots(candidate, header);
        uint16_t free_slot = header.slot_count;
        for (uint16_t i = 0; i < header.slot_count; ++i)
            if ((read_slot(candidate, i).flags & occupied_flag) == 0)
            {
                free_slot = i;
                break;
            }
        const size_t directory_cost = free_slot == header.slot_count ? slot_size : 0;
        if (header_size + static_cast<size_t>(header.slot_count) * slot_size +
            directory_cost + live_bytes(candidate, header) + payload.size() <=
            pager::page_size)
        {
            compact(candidate, header);
            if (free_slot == header.slot_count)
            {
                if (header.slot_count > slot_mask)
                    throw runtime_error("Too many slots in a row-store page.");
                ++header.slot_count;
                header.free_start = static_cast<uint16_t>(header.free_start + slot_size);
            }
            row_slot slot = read_slot(candidate, free_slot);
            slot.generation = static_cast<uint16_t>(slot.generation + 1);
            if (slot.generation == 0) ++slot.generation;
            header.free_end = static_cast<uint16_t>(header.free_end - payload.size());
            slot.offset = header.free_end;
            slot.length = static_cast<uint16_t>(payload.size());
            slot.flags = new_flags;
            if (!payload.empty())
                memcpy(candidate.data() + slot.offset, payload.data(), payload.size());
            ++header.live_count;
            write_slot(candidate, free_slot, slot);
            write_header(candidate, header);
            tx.write(page_number) = candidate;
            return make_id(page_number, free_slot, slot.generation);
        }
        page_number = header.next_page;
    }

    page_number = tx.allocate();
    pager::page page{};
    row_page_header header{1, 1, static_cast<uint16_t>(header_size + slot_size),
                           static_cast<uint16_t>(pager::page_size - payload.size()),
                           tx.row_page()};
    row_slot slot{header.free_end, static_cast<uint16_t>(payload.size()), 1, new_flags};
    if (!payload.empty()) memcpy(page.data() + slot.offset, payload.data(), payload.size());
    write_slot(page, 0, slot);
    write_header(page, header);
    tx.write(page_number) = page;
    tx.set_row_page(page_number);
    return make_id(page_number, 0, 1);
}

optional<vector<uint8_t>> row_store::get(b_tree_read_transaction& transaction,
                                         row_id id) const
{
    return get_row([&](uint64_t page) { return transaction._tree->_pager.read(page); }, id);
}

optional<vector<uint8_t>> row_store::get(b_tree_write_transaction& transaction,
                                         row_id id) const
{
    transaction.require_active();
    return get_row([&](uint64_t page) -> const pager::page& {
        return transaction._transaction.read(page);
    }, id);
}

bool row_store::remove(b_tree_write_transaction& transaction, row_id id) const
{
    transaction.require_active();
    const uint64_t page_number = id_page(id);
    if (page_number == 0 || id_generation(id) == 0) return false;
    pager::page page = transaction._transaction.read(page_number);
    row_page_header header = read_header(page);
    validate_slots(page, header);
    const uint16_t index = id_slot(id);
    if (index >= header.slot_count) return false;
    row_slot slot = read_slot(page, index);
    if ((slot.flags & occupied_flag) == 0 || slot.generation != id_generation(id))
        return false;
    if ((slot.flags & overflow_flag) != 0)
    {
        uint64_t chain = read_overflow_stub(page, slot).first;
        while (chain != 0)
        {
            const uint64_t next =
                read_overflow_header(transaction._transaction.read(chain)).next_page;
            transaction._transaction.release(chain);
            chain = next;
        }
    }
    slot.flags = 0;
    slot.offset = 0;
    slot.length = 0;
    --header.live_count;
    write_slot(page, index, slot);
    write_header(page, header);
    transaction._transaction.write(page_number) = page;
    return true;
}
