/******************************************************************************/
/**
@file		btree.c
@author		Ramon Lawrence
@brief		Implementation of B-Tree for small memory embedded devices.
@copyright	Copyright 2021
			The University of British Columbia,		
@par Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

@par 1.Redistributions of source code must retain the above copyright notice,
	this list of conditions and the following disclaimer.

@par 2.Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

@par 3.Neither the name of the copyright holder nor the names of its contributors
	may be used to endorse or promote products derived from this software without
	specific prior written permission.

@par THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "btree.h"


/*
Comparison functions. Code is adapted from ldbm.
*/
/**
@brief     	Compares two unsigned int32_t values.
@param     	a
                value 1
@param     	b
                value 2
*/
static int8_t uint32Compare(void *a, void *b)
{
	int32_t result = *((int32_t*)a) - *((int32_t*)b);
	if(result < 0) return -1;
	if(result > 0) return 1;
    return 0;	
}

/**
@brief     	Compares two values by bytes. 
@param     	a
                value 1
@param     	b
                value 2
*/
static int8_t byteCompare(void *a, void *b, int16_t size)
{
	return memcmp(a, b, size);	
}


/**
@brief     	Initialize a btree structure.
@param     	state
                btree algorithm state structure
*/
void btreeInit(btreeState *state)
{
	printf("Initializing btree.\n");
	printf("Buffer size: %d  Page size: %d\n", state->buffer->numPages, state->buffer->pageSize);	
	state->recordSize = state->keySize + state->dataSize;
	printf("Record size: %d\n", state->recordSize);	
	
	dbbufferInit(state->buffer);

	state->compareKey = uint32Compare;

	/* Calculate block header size */
	/* Header size fixed: 8 bytes: 4 byte id and 4 for record count. */	
	state->headerSize = 8;

	/* Calculate number of records per page */
	state->maxRecordsPerPage = (state->buffer->pageSize - state->headerSize) / state->recordSize;
	/* Interior records consist of key and id reference. Note: One extra id reference (child pointer). If N keys, have N+1 id references (pointers). */
	state->maxInteriorRecordsPerPage = (state->buffer->pageSize - state->headerSize - sizeof(id_t)) / (state->keySize+sizeof(id_t));

	/* Hard-code for testing */
	state->maxRecordsPerPage = 5;
	state->maxInteriorRecordsPerPage = 4;	

	state->levels = 1;
	state->numMappings = 0;
	state->numNodes = 1;

	/* Create and write empty root node */
	void *buf = initBufferPage(state->buffer, 0);
	BTREE_SET_ROOT(buf);	
	state->activePath[0] = writePage(state->buffer, buf);		/* Store root location */		
}


/**
@brief     	Return the smallest key in the node
@param     	state
                btree algorithm state structure
@param     	buffer
                In memory page buffer with node data
*/
void* btreeGetMinKey(btreeState *state, void *buffer)
{
	return (void*) (buffer+state->headerSize);
}

/**
@brief     	Return the smallest key in the node
@param     	state
                btree algorithm state structure
@param     	buffer
                In memory page buffer with node data
*/
void* btreeGetMaxKey(btreeState *state, void *buffer)
{
	int16_t count =  BTREE_GET_COUNT(buffer); 
	if (count == 0)
		count = 1;		/* Force to have value in buffer. May not make sense but likely initialized to 0. */
	return (void*) (buffer+state->headerSize+(count-1)*state->recordSize);
}

/**
@brief     	Print a node in an in-memory buffer.
@param     	state
                btree algorithm state structure
@param     	pageNum
                Physical page id (number)	
@param     	depth
                Used for nesting print out
@param     	buffer
                In memory page buffer with node data
*/
void btreePrintNodeBuffer(btreeState *state, int pageNum, int depth, void *buffer)
{
	int16_t c, count =  BTREE_GET_COUNT(buffer); 

	if (BTREE_IS_INTERIOR(buffer) && state->levels != 1)
	{		
		printf("%*cId: %d Loc: %d Cnt: %d [%d, %d]\n", depth*3, ' ', BTREE_GET_ID(buffer), pageNum, count, (BTREE_IS_ROOT(buffer)), BTREE_IS_INTERIOR(buffer));		
		/* Print data records (optional) */	
		printf("%*c", depth*3+2, ' ');	
		for (c=0; c < count && c < state->maxInteriorRecordsPerPage; c++)
		{			
			int32_t key = *((int32_t*) (buffer+state->keySize * c + state->headerSize));
			int32_t val = *((int32_t*) (buffer+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(id_t)));			
			printf(" (%d, %d)", key, val);						
		}
		/* Print last pointer */
		int32_t val = *((int32_t*) (buffer+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(id_t)));		
		printf(" (, %d)", val);		
	}
	else
	{		
		printf("%*cId: %d Loc: %d Cnt: %d (%d, %d)\n", depth*3, ' ', BTREE_GET_ID(buffer), pageNum, count, *((int32_t*) btreeGetMinKey(state, buffer)), *((int32_t*) btreeGetMaxKey(state, buffer)));
		/* Print data records (optional) */
		/*
		for (int c=0; c < count; c++)
		{
			int32_t key = *((int32_t*) (buffer + state->headerSize + state->recordSize * c));
			int32_t val = *((int32_t*) (buffer + state->headerSize + state->recordSize * c + state->keySize));
			printf("%*cKey: %d Value: %d\n", depth*3+2, ' ', key, val);			
		}	
		*/				
	}
}

/**
@brief     	Print a node read from storage.
@param     	state
                btree algorithm state structure
@param     	pageNum
                Physical page id (number)	
@param     	depth
                Used for nesting print out
*/
void btreePrintNode(btreeState *state, int pageNum, int depth)
{
	void* buf = readPage(state->buffer, pageNum);
	if (buf == NULL)
	{
		printf("ERROR printing tree. PageNum: %d\n", pageNum);
		return;
	}
	int16_t c, count =  BTREE_GET_COUNT(buf); 	
	
	/* Track number of nodes at this level */
	state->activePath[depth+1]++;

	btreePrintNodeBuffer(state, pageNum, depth, buf);
	if (BTREE_IS_INTERIOR(buf) && state->levels != 1)
	{				
		for (c=0; c < count && c < state->maxInteriorRecordsPerPage; c++)
		{
			int32_t key = *((int32_t*) (buf+state->keySize * c + state->headerSize));
			int32_t val = *((int32_t*) (buf+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(id_t)));
			
			btreePrintNode(state, val, depth+1);				
			buf = readPage(state->buffer, pageNum);			
		}	
		/* Print last child node if active */
		int32_t val = *((int32_t*) (buf+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(id_t)));
		if (val != 0)	/* TODO: Better way to check for invalid node */
		{			
			btreePrintNode(state, val, depth+1);	
		}
	}	
}


/**
@brief     	Print current btree as written on storage.
@param     	state
                btree algorithm state structure
*/
void btreePrint(btreeState *state)
{	
	printf("\n\nPrint tree:\n");

	/* Use active path to keep track of stats of nodes at each level */
	for (count_t l=1; l <= state->levels; l++)
		state->activePath[l] = 0;
	btreePrintNode(state, state->activePath[0], 0);

	/* Print out number of nodes per level */
	count_t total = 0;	
	for (count_t l=1; l <= state->levels; l++)
	{	printf("Nodes level %d: %d\n", l, state->activePath[l]);
		total += state->activePath[l];
	}
	printf("Total nodes: %d (%d)\n", total, state->numNodes);
}

/**
@brief     	Puts a given key, data pair into structure.
@param     	state
                btree algorithm state structure
@param     	key
                Key for record
@param     	data
                Data for record
@return		Return 0 if success. Non-zero value if error.
*/
int8_t btreePut(btreeState *state, void* key, void *data)
{
	/* Check for capacity. TODO: Determine proper cutoff. For now, number of nodes must be less than total possible nodes - 50% */
	/*
	if (state->numNodes >= state->buffer->endDataPage*0.5)
	{
		printf("Storage is at capacity. Must delete keys.\n");
		return -1;
	}		
	*/
	
	int8_t 	l;
	void 	*next, *buf, *ptr;	
	id_t  	parent, nextId = state->activePath[0];	
	int32_t pageNum, childNum;

	/* Find insert leaf */
	/* Starting at root search for key */
	for (l=0; l < state->levels-1; l++)
	{			
		buf = readPage(state->buffer, nextId);		

		// Find the key within the node. Sorted by key. Use binary search. 
		childNum = btreeSearchNode(state, buf, key, nextId, 1);
		nextId = getChildPageId(state, buf, nextId, l, childNum);		
		if (nextId == -1)
			return -1;		
					
		state->activePath[l+1] = nextId;
	}

	/* Read the leaf node */
	buf = readPageBuffer(state->buffer, nextId, 0);	/* Note: Use readPageBuffer in buffer 0 to prevent any concurrency issues instead of readPage. */
	int16_t count =  BTREE_GET_COUNT(buf); 
	state->nodeSplitId = nextId;

	childNum = -1;
	if (count > 0)
		childNum = btreeSearchNode(state, buf, key, nextId, 1);
			
	ptr = buf + state->headerSize + state->recordSize * childNum;
	if (count < state->maxRecordsPerPage)
	{	/* Space for record on leaf node. */		
		/* Insert record onto page in sorted order */		
		/* Shift records down */
		if (count-childNum-1 > 0)
		{
			memcpy(ptr + state->recordSize, ptr, state->recordSize*(count-childNum));					
		}

		childNum++;		/* After search child number is the location where the value is <= key. Increase by one for insert location. */
		/* Copy record onto page */
		ptr += state->recordSize;
		memcpy(ptr, key, state->keySize);
		memcpy(ptr + state->keySize, data, state->dataSize);

		/* Update count */
		BTREE_INC_COUNT(buf);	

		/* Write updated page */
		pageNum = overWritePage(state->buffer, buf, nextId);		
		if (state->levels == 1)
		{	/* Wrote to root */
			state->activePath[0] = pageNum;
		}
		
		return 0;
	}

	/* Current leaf page is full. Perform split. */
	int8_t mid = count/2;
	id_t left, right;
	state->numNodes++;	

	if (childNum < mid)
	{	/* Insert key in page with smaller values */
		/* Update count on page then write */
		BTREE_SET_COUNT(buf, mid+1);	

		/* Buffer key/data record at mid point so do not lose it */
		ptr = buf + state->headerSize + state->recordSize * mid;
		memcpy(state->tempKey, ptr, state->keySize);
		memcpy(state->tempData, ptr + state->keySize, state->dataSize);

		/* Shift records at and after insert point down one record */
		ptr =  buf + state->headerSize + state->recordSize * (childNum+1);
		if ((mid-childNum-1) > 0)
			memcpy(ptr + state->recordSize, ptr, state->recordSize*(mid-childNum-1));		

		/* Copy record onto page */
		memcpy(ptr, key, state->keySize);
		memcpy(ptr + state->keySize, data, state->dataSize);

		left = overWritePage(state->buffer, buf, nextId);	

		/* Copy buffered record to start of block */
		memcpy(buf + state->headerSize, state->tempKey, state->keySize);
		memcpy(buf + state->headerSize + state->keySize, state->tempData, state->dataSize);

		/* Copy records after mid to start of page */	
		memcpy(buf + state->headerSize + state->recordSize, buf + state->headerSize + state->recordSize * (mid+1), state->recordSize*(count-mid));		
		
		BTREE_SET_COUNT(buf, count-mid);
		right = writePage(state->buffer, buf);
	}
	else
	{	/* Insert key in page with larger values */
		/* Update count on page then write */
		BTREE_SET_COUNT(buf, mid+1);

		left = overWritePage(state->buffer, buf, nextId);	

		/* Buffer key/data record at mid point so do not lose it */
		if (childNum == mid)
		{	/* Middle key to promote is this key. */
			memcpy(state->tempKey, key, state->keySize);
		}
		else
		{
			ptr =  buf + state->headerSize + state->recordSize * (mid+1);
			memcpy(state->tempKey, ptr, state->keySize);
		}
		
		/* New split page starts off with original page in buffer. Copy records around as required. */
		/* Copy records before insert point into front of block from current location in block */
		if ((childNum-mid) > 0)
			memcpy(buf + state->headerSize, ptr, state->recordSize*(childNum-mid));		

		/* Copy record onto page */
		ptr = buf + state->headerSize + state->recordSize * (childNum-mid);
		memcpy(ptr, key, state->keySize);
		memcpy(ptr + state->keySize, data, state->dataSize);

		/* Copy records after insert point after value just inserted */
		memcpy(buf + state->headerSize + state->recordSize * (childNum-mid+1), buf + state->headerSize + state->recordSize * (childNum+1), state->recordSize*(count-childNum-1));	

		BTREE_SET_COUNT(buf, count-mid);
		right = writePage(state->buffer, buf);		
	}		

	/* Recursively add pointer to parent node. */
	for (l=state->levels-2; l >=0; l--)
	{		
		parent = state->activePath[l];				

		// printf("Here: Left: %d  Right: %d Key: %d  Parent: %d", left, right, *((int32_t*) state->tempKey), parent);

		/* Read parent node */
		buf = readPageBuffer(state->buffer, parent, 0);			/* Forcing read to buffer 0 even if buffered in another buffer as will modify this page. */
		if (buf == NULL)
			return -1;				

		int16_t count =  BTREE_GET_COUNT(buf); 
		if (count < state->maxInteriorRecordsPerPage)
		{	/* Space for key/pointer in page */
			childNum = btreeSearchNode(state, buf, state->tempKey, parent, 1);		
	
			/* Note: memcpy with overlapping ranges. May be an issue on some platforms */
			ptr = buf + state->headerSize + state->keySize * (childNum);
			/* Shift down all keys */
			memcpy(ptr + state->keySize, ptr, state->keySize * (count-childNum));		

			/* Insert key in page */			
			memcpy(ptr, state->tempKey, state->keySize);

			/* Shift down all pointers */
			ptr = buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * childNum;
			memcpy(ptr + sizeof(id_t), ptr, sizeof(id_t)*(count-childNum+1));

			/* Insert pointer in page */			
			memcpy(ptr, &left, sizeof(id_t));
			memcpy(ptr + sizeof(id_t), &right, sizeof(id_t));

			BTREE_INC_COUNT(buf);			

			/* Write page */			
			pageNum = overWritePage(state->buffer, buf, parent);
			
			if (l == 0)
			{	/* Update root */
				state->activePath[0] = pageNum;
			}			

			return 0;
		}

		/* No space. Split interior node and promote key/pointer pair */
		state->numNodes++;
		
		childNum = -1;
		if (count > 0)
			childNum = btreeSearchNode(state, buf, state->tempKey, parent, 1);
 		mid = count/2;

		if (childNum < mid)
		{	/* Insert key/pointer in page with smaller values */
			/* Update count on page then write */
			if (count % 2 == 0)
				mid--;	/* Note: If count is odd, then first node will have extra key/pointer */
			BTREE_SET_COUNT(buf, mid + 1);	 			
			BTREE_SET_INTERIOR(buf);  

			/* Buffer key/pointer record at mid point so do not lose it */
			/* TODO: Using tempData here as already using tempKey. This would be a problem if data size is < key size. */
			memcpy(state->tempData, buf + state->headerSize + state->keySize * (mid), state->keySize);
			id_t tempPtr;
			memcpy(&tempPtr, buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * (mid+1), sizeof(id_t));

			/* Copy keys and pointers after insert point down one from current location in block */
			ptr = buf + state->headerSize + state->keySize * childNum;
			if ((mid-childNum) > 0)
			{
				/* Shift down all keys */
				memcpy(ptr + state->keySize, ptr, state->keySize*(mid-childNum));

				/* Shift down all pointers */
				ptr = buf + state->headerSize  + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * (childNum+1);
				memcpy(ptr + sizeof(id_t), ptr, sizeof(id_t)*(mid-childNum));		
			}				

			/* Copy record onto page */
			ptr = buf + state->headerSize + state->keySize * childNum;
			memcpy(ptr, state->tempKey, state->keySize);
			ptr = buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * (childNum);
			memcpy(ptr, &left, sizeof(id_t));
			memcpy(ptr + sizeof(id_t), &right, sizeof(id_t));

			left = overWritePage(state->buffer, buf, parent);				
					
			/* Copy buffered pointer to start of block */			
			ptr = buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage;
			memcpy(ptr, &tempPtr, sizeof(id_t));

			/* Copy records after mid to start of page */	
			memcpy(buf + state->headerSize, buf + state->headerSize + state->keySize * (mid+1), state->keySize*(count-mid-1));			
			memcpy(ptr + sizeof(id_t), ptr + sizeof(id_t) * (mid+2), sizeof(id_t)*(count-mid-1));		
			
			BTREE_SET_COUNT(buf, count-mid-1);
			BTREE_SET_INTERIOR(buf);

			right = writePage(state->buffer, buf);			

			/* Keep temporary key (move from temp data) */
			memcpy(state->tempKey, state->tempData, state->keySize);
		}
		else
		{	/* Insert key/pointer in page with larger values */
			/* Update count on page then write */
			BTREE_SET_COUNT(buf, mid);
			BTREE_SET_INTERIOR(buf);					

			ptr = buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage;
			if (childNum == mid)
			{	/* Promote current key that just got promoted. */
				memcpy(state->tempData, state->tempKey, state->keySize);
				/* Left pointer is last pointer in the first node */
				memcpy(ptr + sizeof(id_t) * mid, &left, sizeof(id_t));
			}
			else
			{
				/* TODO: Using tempData here as already using tempKey. This would be a problem if data size is < key size. */
				memcpy(state->tempData, buf + state->headerSize + state->keySize * (mid), state->keySize);
			}
			id_t tempPtr;
			memcpy(&tempPtr, buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * (mid+1), sizeof(id_t));
			
			id_t tmpLeft = overWritePage(state->buffer, buf, parent);				
						
			/* New split page starts off with original page in buffer. Copy records around as required. */
			/* Copy records before insert point into front of block from current location in block */			
			if ((childNum-mid-1) > 0)
			{
				memcpy(buf + state->headerSize, buf + state->headerSize + state->keySize * (mid+1), state->keySize*(childNum-mid-1));	
				memcpy(ptr, ptr + sizeof(id_t) * (mid+1), sizeof(id_t)*(childNum-mid-1));		
			}	  
	 			
			if (childNum > mid)
			{
				/* Copy record onto page */
				memcpy(buf + state->headerSize + state->keySize * (childNum-mid-1), state->tempKey, state->keySize);
				/* Right pointer */
				memcpy(ptr + sizeof(id_t) * (childNum-mid-1), &left, sizeof(id_t));
			}
			memcpy(ptr + sizeof(id_t) * (childNum-mid), &right, sizeof(id_t));

			/* Copy records after insert point after value just inserted */
			if (count-childNum > 0)
			{
				memcpy(buf + state->headerSize + state->keySize * (childNum-mid), buf + state->headerSize + state->keySize * (childNum), state->keySize*(count-childNum));	
				memcpy(ptr + sizeof(id_t) * (childNum-mid+1), ptr + sizeof(id_t) * (childNum+1), sizeof(id_t)*(count-childNum));	
			}
	
			BTREE_SET_COUNT(buf, count-mid);
			BTREE_SET_INTERIOR(buf);

			right = writePage(state->buffer, buf);

			/* Keep temporary key (move from temp data) */
			left = tmpLeft;
			memcpy(state->tempKey, state->tempData, state->keySize);
		}
	}
	
	/* Special case: Add new root node. */	
	/* Create new root node with the two pointers */
	buf = initBufferPage(state->buffer, 0);	
	BTREE_SET_COUNT(buf, 1);
	BTREE_SET_ROOT(buf);			
	state->numNodes++;
	
	/* Add key and two pointers */
	memcpy(buf + state->headerSize, state->tempKey, state->keySize);
	ptr = buf + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage;
	memcpy(ptr, &left, sizeof(id_t));
	memcpy(ptr + sizeof(id_t), &right, sizeof(id_t));

	state->activePath[0] = writePage(state->buffer, buf);
	state->levels++;
	// btreePrintNodeBuffer(state, state->activePath[0], 0, buf);
	return 0;
}

/**
@brief     	Given a key, searches the node for the key.
			If interior node, returns child record number containing next page id to follow.
			If leaf node, returns if of first record with that key or (<= key).
			Returns -1 if key is not found.			
@param     	state
                btree algorithm state structure
@param     	buffer
                Pointer to in-memory buffer holding node
@param     	key
                Key for record
@param		pageId
				Page if for page being searched
@param		range
				1 if range query so return pointer to first record <= key, 0 if exact query so much return first exact match record
*/
int32_t btreeSearchNode(btreeState *state, void *buffer, void* key, id_t pageId, int8_t range)
{
	int16_t first, last, middle, count;
	int8_t compare, interior;
	void *mkey;
	
	count = BTREE_GET_COUNT(buffer);  
	interior = BTREE_IS_INTERIOR(buffer) && state->levels != 1;

	if (interior)
	{
		if (count == 0)	/* Only one child pointer */
			return 0;
		if (count == 1)	/* One key and two children pointers */
		{
			mkey = buffer+state->headerSize;   /* Key at index 0 */
			compare = state->compareKey(key, mkey);
			if (compare < 0)
				return 0;
			return 1;		
		}
		
		first = 0;	
  		last =  count;
		if (last > state->maxInteriorRecordsPerPage)
			last = state->maxInteriorRecordsPerPage;
  		middle = (first+last)/2;
		while (first < last) 
		{			
			mkey = buffer+state->headerSize+state->keySize*middle;
			compare = state->compareKey(key,mkey);
			if (compare > 0)
				first = middle + 1;
			else if (compare == 0) 
			{	last = middle+1; /* Return the child pointer just after */
				break;
			}				
			else
				last = middle;  /* Note: Not -1 as always want last pointer to be <= key so that will use it if necessary */

			middle = (first + last)/2;
		}
		return last;		
	}
	else
	{
		first = 0;	
  		last =  count - 1;
  		middle = (first+last)/2;	

		while (first <= last) 
		{			
			mkey = buffer+state->headerSize+state->recordSize*middle;
			compare = state->compareKey(mkey, key);
			if (compare < 0)
				first = middle + 1;
			else if (compare == 0) 
				return middle;							
			else
				last = middle - 1;

			middle = (first + last)/2;
		}
		if (range)			
		{	// return middle;
			if (last == -1)
				return -1;
			return middle;
		}
		return -1;
	}
}

/**
@brief     	Given a child link, returns the proper physical page id.
			This method handles the mapping of the active path where the pointer in the
			node is not actually pointing to the most up to date block.			
@param     	state
                btree algorithm state structure
@param		buf
				Buffer containing node
@param     	pageId
                Page id for node
@param     	level
                Level of node in tree
@param		childNum
				Child pointer index
@return		Return pageId if success or -1 if not valid.
*/
id_t getChildPageId(btreeState *state, void *buf, id_t pageId, int8_t level, id_t childNum)
{		
	/* Retrieve page number for child */
	id_t nextId = *((id_t*) (buf + state->headerSize + state->keySize*state->maxInteriorRecordsPerPage + sizeof(id_t)*childNum));
	if (nextId == 0 && childNum==(BTREE_GET_COUNT(buf)))	/* Last child which is empty */
		return -1;
	
	return nextId;
}

/**
@brief     	Given a key, returns data associated with key.
			Note: Space for data must be already allocated.
			Data is copied from database into data buffer.
@param     	state
                btree algorithm state structure
@param     	key
                Key for record
@param     	data
                Pre-allocated memory to copy data for record
@return		Return 0 if success. Non-zero value if error.
*/
int8_t btreeGet(btreeState *state, void* key, void *data)
{
	/* Starting at root search for key */
	int8_t l;
	void* next, *buf;
	id_t childNum, nextId = state->activePath[0];
	
	for (l=0; l < state->levels-1; l++)
	{		
		buf = readPage(state->buffer, nextId);		

		/* Find the key within the node. Sorted by key. Use binary search. */
		childNum = btreeSearchNode(state, buf, key, nextId, 0);
		nextId = getChildPageId(state, buf, nextId, l, childNum);
		if (nextId == -1)
			return -1;		
	}

	/* Search the leaf node and return search result */
	buf = readPage(state->buffer, nextId);
	if (buf == NULL)
		return -1;
	nextId = btreeSearchNode(state, buf, key, nextId, 0);
	if (nextId != -1)
	{	/* Key found */
		memcpy(data, (void*) (buf+state->headerSize+state->recordSize*nextId+state->keySize), state->dataSize);
		return 0;
	}
	return -1;
}

/**
@brief     	Initialize iterator on btree structure.
@param     	state
                btree algorithm state structure
@param     	it
                btree iterator state structure
*/
void btreeInitIterator(btreeState *state, btreeIterator *it)
{	
	/* Find start location */
	/* Starting at root search for key */
	int8_t l;
	void* next, *buf;	
	id_t childNum, nextId = state->activePath[0];
	it->currentBuffer = NULL;

	for (l=0; l < state->levels-1; l++)
	{		
		it->activeIteratorPath[l] = nextId;		
		buf = readPage(state->buffer, nextId);		

		/* Find the key within the node. Sorted by key. Use binary search. */
		childNum = btreeSearchNode(state, buf, it->minKey, nextId, 1);
		nextId = getChildPageId(state, buf, nextId, l, childNum);
		if (nextId == -1)
			return;	
		
		it->lastIterRec[l] = childNum;
	}

	/* Search the leaf node and return search result */
	it->activeIteratorPath[l] = nextId;	
	buf = readPage(state->buffer, nextId);
	it->currentBuffer = buf;
	childNum = btreeSearchNode(state, buf, it->minKey, nextId, 1);		
	it->lastIterRec[l] = childNum;
}


/**
@brief     	Requests next key, data pair from iterator.
@param     	state
                btree algorithm state structure
@param     	it
                btree iterator state structure
@param     	key
                Key for record (pointer returned)
@param     	data
                Data for record (pointer returned)
*/
int8_t btreeNext(btreeState *state, btreeIterator *it, void **key, void **data)
{	
	void *buf = it->currentBuffer;
	int8_t l=state->levels-1;
	id_t nextPage;

	/* No current page to search */
	if (buf == NULL)
		return 0;

	/* Iterate until find a record that matches search criteria */
	while (1)
	{	
		if (it->lastIterRec[l] >= BTREE_GET_COUNT(buf))
		{	/* Read next page */						
			it->lastIterRec[l] = 0;

			while (1)
			{
				/* Advance to next page. Requires examining active path. */
				for (l=state->levels-2; l >= 0; l--)
				{	
					buf = readPage(state->buffer, it->activeIteratorPath[l]);
					if (buf == NULL)
						return 0;						

					int8_t count = BTREE_GET_COUNT(buf);
					if (l == state->levels-1)
						count--;
					if (it->lastIterRec[l] < count)
					{
						it->lastIterRec[l]++;
						break;
					}
					it->lastIterRec[l] = 0;
				}
				if (l == -1)
					return 0;		/* Exhausted entire tree */

				for ( ; l < state->levels-1; l++)
				{						
					nextPage = it->activeIteratorPath[l];
					nextPage = getChildPageId(state, buf, nextPage, l, it->lastIterRec[l]);
					if (nextPage == -1)
						return 0;	
					
					it->activeIteratorPath[l+1] = nextPage;
					buf = readPage(state->buffer, nextPage);
					if (buf == NULL)
						return 0;	
				}
				it->currentBuffer = buf;

				/* TODO: Check timestamps, min/max, and bitmap to see if query range overlaps with range of records	stored in block */
				/* If not read next block */
			//	if (btree_USING_BMAP(state->parameters))
				{
					uint8_t bm = 0; // BTREE_GET_BITMAP(state, buf);
					/* TODO: Need to make bitmap comparison more generic. */
					// if ( ( *((uint8_t*) it->queryBitmap) & bm) >= 1)
					{	/* Overlap in bitmap - go to next page */
						break;
					}
				//	else
					{
					//	printf("Skipping page as no bitmap overlap\n");
					}					
				}
				//else
			//		break;
			}
		}
		
		/* Get record */	
		// btreePrintNodeBuffer(state, 0, 0, buf);
		*key = buf+state->headerSize+it->lastIterRec[l]*state->recordSize;
		*data = *key+state->keySize;
		it->lastIterRec[l]++;
		
		/* Check that record meets filter constraints */
		if (it->minKey != NULL && state->compareKey(*key, it->minKey) < 0)
			continue;
		if (it->maxKey != NULL && state->compareKey(*key, it->maxKey) > 0)
			return 0;	/* Passed maximum range */
		return 1;
	}
}


/**
@brief     	Given a physical page number, returns 0 if valid, -1 if no longer used.
@param     	state
                btree algorithm state structure
@param		pageNum
				Physical page number
@param		parentId
				Physical page number of parent
@param		parentBuffer
				Returns pointer to buffer containing parent node if found, NULL otherwise.
@return		Returns 0 if page is valid, -1 if no longer used.
*/
int8_t btreeIsValid(void *statePtr, id_t pageNum, id_t *parentId, void **parentBuffer)
{	
	btreeState *state = statePtr;
	*parentId = 0;	
	*parentBuffer = NULL;

	/* Search to see if page is still in tree */
	void *buf = readPage(state->buffer, pageNum);
	if (buf == NULL)
		return -1;

	/* TODO: Cannot be hardcoded key. Cannot use state->tempKey. */
	int32_t key;
	/* Retrieve minimum key to search for */
	/* Copying tree off page as will need to read other pages and will most likely lose page in buffer */
	memcpy(&key, btreeGetMinKey(state, buf), state->keySize);

	/* This code is almost identical to btreeGet as searching but duplicated it for now as can stop early if find node. */
	/* May be a candidate for code refactorization to avoid this duplication. */
	/* Starting at root search for key */
	int8_t l;	
	id_t childNum, nextId = state->activePath[0];	

	if (nextId == pageNum)
		return -1;		

	for (l=0; l < state->levels-1; l++)
	{		
		buf = readPage(state->buffer, nextId);		

		/* Find the key within the node. */
		childNum = btreeSearchNode(state, buf, &key, nextId, 0);
		*parentId = nextId;
		*parentBuffer = buf;
		nextId = getChildPageId(state, buf, nextId, l, childNum);
		if (nextId == pageNum)						
			return 0;		/* Page is valid page currently in tree */		
		if (nextId == -1)
			break;
	}
	
	return -1;
}


/**
@brief     	Informs the btree that the buffer moved a page from prev to curr location.
			It must update any mappings if required.
@param     	state
                btree algorithm state structure
@param		prev
				Previous physical page number
@param		curr
				Previous physical page number
@param		buf
				Buffer containing the page
*/
void btreeMovePage(void *state, id_t prev, id_t curr, void *buf)
{
	
}