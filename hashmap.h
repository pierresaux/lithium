#ifndef __LITHIUM_HASHMAP_H__
#define __LITHIUM_HASHMAP_H__

#include "lithium.h"
#include "hash.h"
#include "list.h"
#include "iterator.h"
namespace li
{
	template <typename _KeyType, typename _ValType, class _Traits = li::traits <_KeyType> >
		class hashmap
		{
			// typedefs
		protected:
			typedef typename _Traits::ptr_t				__hash_ptr_t;
			struct hash_node
			{
				_KeyType			key;
				_ValType			value;
				list_head			slot_head;
				list_head			head;
				hash_node () {}
				hash_node (_KeyType key, _ValType value) :
					key(key), value(value) {}
			};

		public:
			typedef li::list_head_iterator <hash_node, _ValType> iterator;
			friend class li::list_head_iterator <hash_node, _ValType>;

			// public methods
		public:
			hashmap () : _size (4096) { _post_ctor (); }

			hashmap (size_t size) : _size (size) { _post_ctor (); }

			~hashmap () {}

			li::pair <iterator, bool> insert (const _KeyType &_key, const _ValType &val)
			{
				__hash_ptr_t keyptr = _Traits::get_key_ptr (_key);
				index_t _x = _hash ((u8 *)keyptr, _Traits::get_size (_key)) % _size;
				hash_node *node;

				/* list_for_each_entry */
				for (list_head *root = _table + _x, *p = root->next;
						p != root; 
						p = p->next)
				{
					node = container_of (p, hash_node, slot_head);
					if (node->key == _key) {
						return li::make_pair (iterator (&node->head), false);
					}
				}

				/* key not found, insert */
				node = new hash_node (_key, val);
				list_insert (_table + _x, &node->slot_head);
				list_insert (&head, &node->head);
				return li::make_pair (iterator (&node->head), true);
			}

			iterator find (const _KeyType &_key)
			{
				__hash_ptr_t keyptr = _Traits::get_key_ptr (_key);
				index_t _x = _hash ((u8 *)keyptr, _Traits::get_size (_key)) % _size;
				hash_node *node;

				/* list_for_each_entry */
				for (list_head *root = _table + _x, *p = root->next;
						p != root; 
						p = p->next)
				{
					node = container_of (p, hash_node, slot_head);
					// found
					if (node->key == _key) {
						return iterator (&node->head);
					}
				}

				// not found
				return end ();
			}

			_ValType& operator[] (const _KeyType &key) { return *(find (key)); }

			iterator begin () {
				return iterator (head.next);
			}

			iterator end () {
				return iterator (&head);
			}

			// protected methods
		protected:
			/**
			 * post-constructor initialization - run after constructor's
			 * initialization list
			 */
			void _post_ctor () {
				_table = new list_head [_size] ();
			}

			// attributes
		protected:
			size_t					_size;
			list_head				*_table;
			list_head				head;
		};
}
#endif // __LITHIUM_HASHMAP_H__
