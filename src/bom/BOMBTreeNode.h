#ifndef BOMBTREENODE_H
#define BOMBTREENODE_H
#include "be.h"
#include "bom.h"
#include "BOMStore.h"
#include <vector>
#include <memory>

class BOMStore;

class BOMBTreeNode
{
public:
	BOMBTreeNode()
	: m_store(nullptr), m_descriptor(nullptr)
	{
	}
	
	BOMBTreeNode(std::shared_ptr<BOMStore> store, BOMNodeDescriptor* descriptor)
	: m_store(store), m_descriptor(descriptor)
	{
	}
	
	BOMBTreeNode(std::shared_ptr<BOMStore> store, uint32_t blockId)
	: m_store(store)
	{
		m_descriptor = m_store->getBlockData<BOMNodeDescriptor>(blockId);
	}
	
	BOMNodeDescriptor* descriptor() { return m_descriptor; }
	
	BOMNodeType kind() const { return BOMNodeType(be(m_descriptor->nodeType)); }
	uint16_t recordCount() const { return be(m_descriptor->numRecords); }
	
	template<typename KeyType>
	KeyType* getKey()
	{
		
	}
	
	template<typename KeyType>
	KeyType* getRecordKey(uint16_t recordIndex) const
	{
		return m_store->getBlockData<KeyType>(be(recordHeader(recordIndex)->recordID));
	}
	
	template<typename DataType>
	DataType* getRecordData(uint16_t recordIndex) const
	{
		return m_store->getBlockData<DataType>(be(recordHeader(recordIndex)->childID));
	}
	
	uint32_t getRecordDataBlockId(uint16_t recordIndex) const
	{
		return be(recordHeader(recordIndex)->childID);
	}
	
	uint32_t forwardLink() const
	{
		return be(m_descriptor->fLink);
	}
	
	bool isInvalid()
	{
		return m_descriptor == nullptr;
	}
	
	template <typename KeyType> class RecordIterator : public std::iterator<std::random_access_iterator_tag, KeyType*>
	{
	public:
		typedef typename std::iterator<std::random_access_iterator_tag, KeyType*>::difference_type difference_type;

		RecordIterator() : m_node(nullptr), m_index(0)
		{
		}
		
		RecordIterator(const RecordIterator& that) : m_node(that.m_node), m_index(that.m_index)
		{
		}
		
		RecordIterator(const BOMBTreeNode* node, int index) : m_node(node), m_index(index)
		{
		}
		
		RecordIterator& operator=(const RecordIterator& that)
		{
			m_node = that.m_node;
			m_index = that.m_index;
			return *this;
		}
		
		KeyType* operator*()
		{
			return m_node->getRecordKey<KeyType>(m_index);
		}
		RecordIterator& operator++()
		{
			m_index++;
			return *this;
		}

		difference_type operator-(const RecordIterator& that)
		{
			return m_index - that.m_index;
		}

		RecordIterator& operator+=(const difference_type& n)
		{
			m_index += n;
			return *this;
		}

		RecordIterator& operator-=(const difference_type& n)
		{
			m_index -= n;
			return *this;
		}

		bool operator!=(const RecordIterator& that)
		{
			return m_index != that.m_index;
		}
		bool operator==(const RecordIterator& that)
		{
			return m_index == that.m_index;
		}
		int index() const
		{
			return m_index;
		}
	private:
		const BOMBTreeNode* m_node;
		int m_index;
	};

	template <typename KeyType> RecordIterator<KeyType> begin() const
	{
		return RecordIterator<KeyType>(this, 0);
	}

	template <typename KeyType> RecordIterator<KeyType> end() const
	{
		return RecordIterator<KeyType>(this, recordCount());
	}
	
private:
	BOMNodeRecordHeader* recordHeader(size_t index) const
	{
		return reinterpret_cast<BOMNodeRecordHeader*>(m_descriptor+1) + index;
	}
private:
	std::shared_ptr<BOMStore> m_store;
	BOMNodeDescriptor* m_descriptor;
};

#endif /* BOMBTREENODE_H */

