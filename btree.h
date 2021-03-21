/******************************************************************************/
/**
@file		btree.h
@author		Ramon Lawrence
@brief		Implementation of B-tree for embedded devices.
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
#if defined(__cplusplus)
extern "C" {
#endif

#ifndef BTREE_H
#define BTREE_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "dbbuffer.h"

/* Define type for page ids (physical and logical). */
typedef uint32_t id_t;

/* Define type for page record count. */
typedef uint16_t count_t;

/* Offsets with header */
#define BTREE_COUNT_OFFSET		sizeof(id_t)

/* MOD 10000 to remove any flags in count that are set above 10000 */
#define BTREE_GET_ID(x)  		*((id_t *) (x)) 
#define BTREE_GET_COUNT(x)  	*((count_t *) (x+BTREE_COUNT_OFFSET)) % 10000
#define BTREE_SET_ID(x,y)  		*((id_t *) (x)) = y
#define BTREE_SET_COUNT(x,y)  	*((count_t *) (x+BTREE_COUNT_OFFSET)) = y
#define BTREE_INC_COUNT(x)  	*((count_t *) (x+BTREE_COUNT_OFFSET)) = *((count_t *) (x+BTREE_COUNT_OFFSET))+1
#define BTREE_GET_VALID(x)		( *((id_t *) (x)) > 10000000 : 0 : 1)
#define BTREE_SET_INVALID(x)	( *((id_t *) (x)) += 10000000 )

/* Using count field above 10000 for interior node and 20000 for root node */
#define BTREE_IS_INTERIOR(x)  	(*((count_t *) (x+BTREE_COUNT_OFFSET)) >= 10000 ? 1 : 0)
#define BTREE_IS_ROOT(x)  		(*((count_t *) (x+BTREE_COUNT_OFFSET)) >= 20000 ? 1 : 0)
#define BTREE_SET_INTERIOR(x) 	BTREE_SET_COUNT(x,*((count_t *) (x+BTREE_COUNT_OFFSET))+10000)
#define BTREE_SET_ROOT(x) 		BTREE_SET_COUNT(x,*((count_t *) (x+BTREE_COUNT_OFFSET))+20000)

#define MAX_LEVEL 8

typedef struct {			
	uint8_t parameters;    						/* Parameter flags */
	uint8_t keySize;							/* Size of key in bytes (fixed-size records) */
	uint8_t dataSize;							/* Size of data in bytes (fixed-size records) */
	uint8_t recordSize;							/* Size of record in bytes (fixed-size records) */
	uint8_t headerSize;							/* Size of header in bytes (calculated during init()) */
	id_t 	nextPageId;							/* Next logical page id. Page id is an incrementing value and may not always be same as physical page id. */
	count_t maxRecordsPerPage;					/* Maximum records per page */
	count_t maxInteriorRecordsPerPage;			/* Maximum interior records per page */
    int8_t (*compareKey)(void *a, void *b);		/* Function that compares two arbitrary keys passed as parameters */	
	uint8_t levels;								/* Number of levels in tree */
	id_t 	activePath[MAX_LEVEL];				/* Active path of page indexes from root (in position 0) to node just above leaf */
	id_t 	nextPageWriteId;					/* Physical page id of next page to write. */
	void 	*tempKey;							/* Used to temporarily store a key value. Space must be preallocated. */
	void 	*tempData;							/* Used to temporarily store a data value. Space must be preallocated. */
	dbbuffer *buffer;							/* Pre-allocated memory buffer for use by algorithm */		
	id_t	numNodes;							/* Total number of nodes in tree */	
} btreeState;

typedef struct {
	id_t 	activeIteratorPath[MAX_LEVEL];		/* Active path of iterator from root (in position 0) to current leaf node */    
	count_t lastIterRec[MAX_LEVEL];				/* Last record processed by iterator at each level */
	void*	minKey;								/* Minimum search key (inclusive) */
	void*	maxKey;    							/* Maximum search key (inclusive) */
	void*   currentBuffer;						/* Current buffer used by iterator */
} btreeIterator;

/**
@brief     	Initialize a BTree structure.
@param     	state
                BTree algorithm state structure
*/
void btreeInit(btreeState *state);

/**
@brief     	Recovers a BTree from storage.
@param     	state
                BTree algorithm state structure
*/
void btreeRecover(btreeState *state);

/**
@brief     	Puts a given key, data pair into structure.
@param     	state
                BTree algorithm state structure
@param     	key
                Key for record
@param     	data
                Data for record
@return		Return 0 if success. Non-zero value if error.
*/
int8_t btreePut(btreeState *state, void* key, void *data);

/**
@brief     	Given a key, returns data associated with key.
			Note: Space for data must be already allocated.
			Data is copied from database into data buffer.
@param     	state
                BTree algorithm state structure
@param     	key
                Key for record
@param     	data
                Pre-allocated memory to copy data for record
@return		Return 0 if success. Non-zero value if error.
*/
int8_t btreeGet(btreeState *state, void* key, void *data);

/**
@brief     	Initialize iterator on BTree structure.
@param     	state
                BTree algorithm state structure
@param     	it
                BTree iterator state structure
*/
void btreeInitIterator(btreeState *state, btreeIterator *it);

/**
@brief     	Requests next key, data pair from iterator.
@param     	state
                BTree algorithm state structure
@param     	it
                BTree iterator state structure
@param     	key
                Key for record (pointer returned)
@param     	data
                Data for record (pointer returned)
*/
int8_t btreeNext(btreeState *state, btreeIterator *it, void **key, void **data);


/**
@brief     	Prints BTree structure to standard output.
@param     	state
                BTree algorithm state structure
*/
void btreePrint(btreeState *state);


/**
@brief     	Given a key, searches the node for the key.
			If interior node, returns child record number containing next page id to follow.
			If leaf node, returns if of first record with that key or (<= key).
			Returns -1 if key is not found.			
@param     	state
                BTree algorithm state structure
@param     	buffer
                Pointer to in-memory buffer holding node
@param     	key
                Key for record
@param		pageId
				Page if for page being searched
@param		range
				1 if range query so return pointer to first record <= key, 0 if exact query so much return first exact match record
*/
int32_t btreeSearchNode(btreeState *state, void *buffer, void* key, id_t pageId, int8_t range);


/**
@brief     	Given a child link, returns the proper physical page id.
			This method handles the mapping of the active path where the pointer in the
			node is not actually pointing to the most up to date block.			
@param     	state
                BTree algorithm state structure
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
id_t getChildPageId(btreeState *state, void *buf, id_t pageId, int8_t level, id_t childNum);


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
void btreePrintNodeBuffer(btreeState *state, id_t pageNum, int depth, void *buffer);

/**
@brief     	Clears statistics.
@param     	state
                BTree algorithm state structure
*/
void btreeClearStats(btreeState *state);

#if defined(__cplusplus)
}
#endif

#endif