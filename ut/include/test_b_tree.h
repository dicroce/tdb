
#include "framework.h"

class test_b_tree : public test_fixture
{
public:
    RTF_FIXTURE(test_b_tree);
      TEST(test_b_tree::test_CAS);
      TEST(test_b_tree::test_basic);
      TEST(test_b_tree::test_duplicate_key_insert);
      TEST(test_b_tree::test_dot_file);
      TEST(test_b_tree::test_basic_remove);
      TEST(test_b_tree::test_lots_of_inserts_and_removes);
      TEST(test_b_tree::test_insert_ascending_order);
      TEST(test_b_tree::test_insert_descending_order);
      TEST(test_b_tree::test_remove_random_order);
      TEST(test_b_tree::test_search_non_existent_keys);
      TEST(test_b_tree::test_large_number_of_keys);
      TEST(test_b_tree::test_concurrent_inserts);
      TEST(test_b_tree::test_delete_and_reinsert);
      TEST(test_b_tree::test_concurrent_readers);
      TEST(test_b_tree::test_remove_reuses_pages);
      TEST(test_b_tree::test_iterator);
      TEST(test_b_tree::test_explicit_write_transaction);
      TEST(test_b_tree::test_row_store);
      TEST(test_b_tree::test_row_store_overflow);
      TEST(test_b_tree::test_row_store_overflow_reuses_pages);
      TEST(test_b_tree::test_row_store_overflow_is_atomic);
      TEST(test_b_tree::test_atomic_row_and_index_transaction);
    RTF_FIXTURE_END();

    virtual ~test_b_tree() throw() {}

    virtual void setup();
    virtual void teardown();

    void test_CAS();
    void test_basic();
    void test_duplicate_key_insert();
    void test_dot_file();
    void test_basic_remove();
    void test_lots_of_inserts_and_removes();
    void test_insert_ascending_order();
    void test_insert_descending_order();
    void test_remove_random_order();
    void test_search_non_existent_keys();
    void test_large_number_of_keys();
    void test_concurrent_inserts();
    void test_delete_and_reinsert();
    void test_concurrent_readers();
    void test_remove_reuses_pages();
    void test_iterator();
    void test_explicit_write_transaction();
    void test_row_store();
    void test_row_store_overflow();
    void test_row_store_overflow_reuses_pages();
    void test_row_store_overflow_is_atomic();
    void test_atomic_row_and_index_transaction();
};
