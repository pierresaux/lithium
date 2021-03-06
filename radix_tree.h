/**
 * This file is part of lithium.
 *
 * lithium is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * lithium is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lithium.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * Radix tree implementation based on linux kernel's radix tree.
 *
 * Author: Pierre Saux
 * First commit date: 3-Jan-2014
 *
 * Changelog:
 * pierresaux (16-jan-14):Refactor, reuse list_head and list_head_iterator
 *
 * You may find the original version in:
 * 
 * Header: $(linux_src)/include/linux/radix_tree.h
 * Source: $(linux_src)/lib/radix_tree.c
 */
#ifndef __LITHIUM_RADIX_TREE_H__
#define __LITHIUM_RADIX_TREE_H__

#include "lithium.h"
#include "list.h"
#include "iterator.h"

namespace li
{
/**
 * Branch factor must be a power of 2. Default to 2^6 (copied from kernel)
 */
#define RT_BRANCH_FACTOR_BIT	(6)
#define RT_BRANCH_FACTOR		(1 << RT_BRANCH_FACTOR_BIT)
#define RT_BRANCH_INDEX_MASK	(RT_BRANCH_FACTOR - 1)

/**
 * radix_tree container
 */
template <typename _ValType>
class radix_tree
{
protected:
	/**
	 * radix tree indicing node
	 */
	typedef pair<index_t, _ValType> val_t;
	struct radix_node
	{
		radix_node				*parent;	// parent slot
		s16						height;		// height of subtree
		u16						size;		// number of occupied slots
		u16						offset;		// parent->slots[offset] == this
		radix_node				*slots [RT_BRANCH_FACTOR];		// branch slots

		radix_node () :	height (0),
				   size (0),
				   offset (0)
		{
		    for (int i = 0; i < RT_BRANCH_FACTOR; ++i)
		 	   slots [i] = NULL;
		    parent = this;
		}

		// dfs delete
		virtual ~radix_node ()
		{
			radix_node *slot;
			if (height >= 0)
				for (int i = 0; i < RT_BRANCH_FACTOR; ++i)
				{
					slot = slots[i];
					if (slot)
						delete slot;
				}
		}
	};

	/**
	 * radix tree data node
	 */
	struct data_node {
		radix_node				*parent;	// parent slot
		list_head				head;		// list_head for iterator and more features
		u16						offset;		// parent->slots[offset] == this
		val_t					value;		// value pair

		// default constructor
		data_node () {}

		// key-value constructor
		data_node (index_t key, _ValType val) : offset (0) 
		{
			value.first = key;
			value.second = val;
		}

		// must override to prevent calling radix_node destructor
		virtual ~data_node () {}
	};

	// types
public:
	typedef list_head_iterator <data_node, val_t> iterator;
	friend struct list_head_iterator <data_node, val_t>;

	typedef const_list_head_iterator <data_node, val_t> const_iterator;
	friend struct const_list_head_iterator <data_node, val_t>;

	// functions
public:
	// default constructor
	radix_tree () : _size (0) { __ctor (); }

	radix_tree (radix_tree &tree) : _size (0)
	{
		__ctor ();
		for (iterator it = tree.begin (); it != tree.end (); ++it)
		{
			val_t value = *it;
			insert (value.first, value.second);
		}
	}

	// dfs start delete
	~radix_tree ()
	{
		radix_node *node = rt_root;
		if (node)
			delete node;
	}

	iterator begin () { return iterator (head.next); }
	const_iterator begin () const { return const_iterator (head.next); }

	iterator end () { return iterator (&head); }
	const_iterator end () const { return const_iterator (&head); }

	bool empty () const { return _size == 0; }

	void clear () 
	{
		for (list_head *p = head.next, *nx;
				p != &head && ((nx = p->next) || 1);
				p = nx)
		{
			erase (iterator (p));
		}
	}

	size_t size () { return _size; }

	/**
	 * Insert key-value pair
	 */
	pair<iterator, bool> insert (index_t key, _ValType val)

	{
		pair<iterator, bool> retval;
		data_node *node;
		radix_node *parent, *slot, **slots;
		register u32 height, index, shift;

		while (key > max_index[rt_root->height])
			rt_root = extend ();

		parent = rt_root;
		slot = NULL;
		slots = (radix_node **)rt_root->slots;
		height = rt_root->height;
		shift = (height) * RT_BRANCH_FACTOR_BIT;
		while (height > 0)
		{
			// retrieve array index in current level
			index = (key >> shift) & (RT_BRANCH_INDEX_MASK);
			slot = slots[index];
			if (slot == NULL)
			{	// extend slot
				slot = new radix_node ();
				slot->height = height - 1;
				slot->parent = parent;
				slot->offset = index;
				slots[index] = slot;
				parent->size++;
			}

			// move down a level
			slots = (radix_node **) slot->slots;
			parent = slot;
			shift -= RT_BRANCH_FACTOR_BIT;
			--height;
		}

		// ready to insert value after DFS
		index = key & (RT_BRANCH_INDEX_MASK);

		// check for existence
		retval.second = slots[index] == NULL;
		if (retval.second)
		{
			// insert entry
			++parent->size;
			++_size;

			node = new data_node (key, val);
			node->parent = parent;
			node->offset = index;
			slots[index] = (radix_node *)node;

			// insert to the iteration list
			list_insert (&head, &node->head);
		}


		// return iterator with the destination item (no matter new/old)
		retval.first = iterator (&node->head);

		return retval;
	}

	/**
	 * find()
	 */
	iterator find (index_t key) {
		return iterator (__find (key));
	}

	const_iterator find (index_t key) const {
		return const_iterator (__find (key));
	}

	/**
	 * find and erase a key
	 */
	u32 erase (const index_t &key)
	{
		iterator it = find (key);
		if (it == end ())
			return 0;
		erase (it);
		return 1;
	}

	/**
	 * erase using an iterator
	 */
	void erase (iterator iter)
	{
		list_head * tmp = iter.p;
		data_node *d_node = container_of (tmp, data_node, head);
		radix_node *p_node = d_node->parent;

		/**
		 * erase item in parent's slots
		 */
		p_node->slots [d_node->offset] = NULL;
		--p_node->size;
		if (p_node->size == 0)
			shrink (p_node);
		delete d_node;

		--_size;
	}

	_ValType& operator[] (index_t key)
	{
		return (find (key))->second;
	}

protected:
	void __ctor ()
	{
		rt_root = new radix_node ();

		// initialize max height
		max_height = (u32) (div_round_up (sizeof (index_t) << 3,
					RT_BRANCH_FACTOR_BIT));

		// initialize max index array
		u64 last = 1;
		for (u32 i = 0; i < max_height; ++i)
		{
			max_index[i] = last << RT_BRANCH_FACTOR_BIT;
			last = max_index[i];
			--max_index[i];
		}
	}

	radix_node * extend ()
	{	// only happen on the root
		radix_node * retval = new radix_node ();
		retval->height = rt_root->height + 1;
		retval->size = 1;
		retval->slots[0] = rt_root;
		rt_root->parent = retval;
		return retval;

	}

	void shrink (radix_node *node)		
	{	// only happen on the root
		while (node != rt_root && node->size == 0)
		{
			radix_node * parent = node->parent;
			parent->slots[node->offset] = NULL;
			delete node;
			node = parent;
		}
	}

	list_head * __find (index_t key) const
	{
		list_head *retval;
		radix_node *slot, **slots;
		data_node *node;
		register u32 height, index, shift;

		if (key > max_index[rt_root->height])
			goto out_not_found;

		slot = NULL;
		slots = (radix_node **)rt_root->slots;
		height = rt_root->height;
		shift = (height) * RT_BRANCH_FACTOR_BIT;
		while (height > 0)
		{
			// retrieve array index in current level
			index = (key >> shift) & (RT_BRANCH_INDEX_MASK);
			slot = slots[index];
			if (slot == NULL)
			{	// not found
				goto out_not_found;
			}

			// move down a level
			slots = (radix_node **) slot->slots;
			shift -= RT_BRANCH_FACTOR_BIT;
			--height;
		}

		// ready to insert value after DFS
		index = key & (RT_BRANCH_INDEX_MASK);

		node = (data_node*) slots[index];
		retval = &node->head;
out:
		return retval;

out_not_found:
		retval = (list_head *) &head;
		goto out;
	}

	// member fields
protected:
	size_t			_size;
	radix_node		*rt_root;
	list_head		head;		// begin() & end()
	u32				max_height;
	index_t			max_index[sizeof(index_t) << 3 /* size of a byte */];
};
} // namespace li
#endif // __LITHIUM_RADIX_TREE_H__
