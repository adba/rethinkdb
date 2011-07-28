#ifndef __BTREE_OPERATIONS_HPP__
#define __BTREE_OPERATIONS_HPP__


#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include "utils.hpp"
#include "buffer_cache/buf_lock.hpp"
#include "containers/scoped_malloc.hpp"
#include "btree/node.hpp"

class btree_slice_t;

/* An abstract superblock provides the starting point for performing btree operations */
class superblock_t {
public:
    superblock_t() { }
    virtual ~superblock_t() { }
    // Release the superblock if possible (otherwise do nothing)
    virtual void release() = 0;
    // If we hold a lock on a super block, swap it into swapee
    // (might swap in an empty buf_lock_t if we don't have an actual superblock)
    virtual void swap_buf(buf_lock_t &swapee) = 0;
    virtual block_id_t get_root_block_id() const = 0;
    virtual void set_root_block_id(const block_id_t new_root_block) = 0;
    virtual block_id_t get_delete_queue_block() const = 0;

private:
    DISABLE_COPYING(superblock_t);
};

/* real_superblock_t implements superblock_t in terms of an actual on-disk block
   structure. */
class real_superblock_t : public superblock_t {
public:
    real_superblock_t(buf_lock_t &sb_buf);

    void release();
    void swap_buf(buf_lock_t &swapee);
    block_id_t get_root_block_id() const;
    void set_root_block_id(const block_id_t new_root_block);
    block_id_t get_delete_queue_block() const;

private:
    buf_lock_t sb_buf_;
};

/* This is for nested btrees, where the "superblock" is really more like a super value.
 It provides an in-memory superblock replacement.

 Note for use for nested btrees: If you want to nest a tree into some super value,
 you would probably have a block_id_t nested_root value in the super value. Then,
 before accessing the nested tree, you can construct a virtual_superblock_t
 based on the nested_root value. Once write operations to the nested btree have
 finished, you should check whether the root_block_id has been changed,
 and if it has, ude get_root_block_id() to update the nested_root value in the
 super block.
 */
class virtual_superblock_t : public superblock_t {
public:
    virtual_superblock_t(block_id_t root_block_id = NULL_BLOCK_ID) : root_block_id_(root_block_id) { }

    void release() { }
    void swap_buf(buf_lock_t &swapee) {
        // Swap with empty buf_lock
        buf_lock_t tmp;
        tmp.swap(swapee);
    }
    block_id_t get_root_block_id() const {
        return root_block_id_;
    }
    void set_root_block_id(const block_id_t new_root_block) {
        root_block_id_ = new_root_block;
    }
    block_id_t get_delete_queue_block() const {
        return NULL_BLOCK_ID;
    }

private:
    block_id_t root_block_id_;
};

class got_superblock_t {
public:
    got_superblock_t() { }

    boost::shared_ptr<transaction_t> txn;
    boost::scoped_ptr<superblock_t> sb;

private:
    DISABLE_COPYING(got_superblock_t);
};

// For read mode operations.
void get_btree_superblock(btree_slice_t *slice, access_t access, order_token_t token, got_superblock_t *got_superblock_out);

void get_btree_superblock(btree_slice_t *slice, access_t access, int expected_change_count, repli_timestamp_t tstamp, order_token_t token, got_superblock_t *got_superblock_out);

template <class Value>
class keyvalue_location_t {
public:
    keyvalue_location_t() : there_originally_was_value(false) { }

    boost::shared_ptr<transaction_t> txn;
    boost::scoped_ptr<superblock_t> sb;

    // The parent buf of buf, if buf is not the root node.  This is hacky.
    buf_lock_t last_buf;

    // The buf owning the leaf node which contains the value.
    buf_lock_t buf;

    bool there_originally_was_value;
    // If the key/value pair was found, a pointer to a copy of the
    // value, otherwise NULL.
    scoped_malloc<Value> value;

private:
    DISABLE_COPYING(keyvalue_location_t);
};

template <class Value>
void find_keyvalue_location_for_write(value_sizer_t<Value> *sizer, got_superblock_t *got_superblock, btree_key_t *key, repli_timestamp_t tstamp, keyvalue_location_t<Value> *keyvalue_location_out);

template <class Value>
void find_keyvalue_location_for_read(value_sizer_t<Value> *sizer, got_superblock_t *got_superblock, btree_key_t *key, keyvalue_location_t<Value> *keyvalue_location_out);

template <class Value>
void apply_keyvalue_change(value_sizer_t<Value> *sizer, keyvalue_location_t<Value> *location_and_value, btree_key_t *key, repli_timestamp_t timestamp);


template <class Value>
class value_txn_t {
public:
    value_txn_t(btree_key_t *, value_sizer_t<Value> *, keyvalue_location_t<Value> *, repli_timestamp_t);
    ~value_txn_t();
    scoped_malloc<Value> value;
private:
    btree_key_t *key;
    value_sizer_t<Value> *sizer;
    keyvalue_location_t<Value> *kv_location;
    repli_timestamp_t tstamp;
};

template <class Value>
value_txn_t<Value> get_value_write(btree_slice_t *, btree_key_t *, repli_timestamp_t, order_token_t);

template <class Value>
void get_value_read(btree_slice_t *, btree_key_t *, order_token_t, keyvalue_location_t<Value> *);

#include "btree/operations.tcc"

#endif  // __BTREE_OPERATIONS_HPP__
