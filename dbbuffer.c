/******************************************************************************/
/**
@file		dbbuffer.c
@author		Ramon Lawrence
@brief		Light-weight buffer implementation for small embedded devices.
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
#include <string.h>

#include "dbbuffer.h"
#include "btree.h"

/**
@brief     	Initializes buffer given page size and number of pages.
@param     	state
                DBbuffer state structure
*/
void dbbufferInit(dbbuffer *state)
{
	printf("Initializing buffer.\n");
	printf("Buffer size: %d  Page size: %d\n", state->numPages, state->pageSize);			
	
	/* TODO: These values would be set during recovery if database already exists. */
	state->nextPageId = 0;
	state->nextPageWriteId = 0;
	state->wrappedMemory = 0;	
	state->endDataPage = (state->endAddress-state->startAddress) / state->pageSize -1;

	state->numReads = 0;
	state->numWrites = 0;
	state->numOverWrites = 0;
	state->bufferHits = 0;
	state->lastHit = 0;
	state->nextBufferPage = 1;

	/* Erase first two blocks. */
	erasePages(state, 0, state->eraseSizeInPages*2-1);
	state->blockEndPage = state->eraseSizeInPages-1;	
	state->erasedStartPage = state->blockEndPage+1;

	for (count_t l=0; l < state->numPages; l++)
		state->status[l] = 0;	
}

/**
@brief      Reads page either from buffer or from storage. Returns pointer to buffer if success.
@param     	state
                DBbuffer state structure
@param     	pageNum
                Physical page id (number)
@return		Returns pointer to buffer page or NULL if error.
*/
void* readPage(dbbuffer *state, id_t pageNum)
{    
	void *buf;
	count_t i;	

	/* Check to see if page is currently in buffer */
	for (i=1; i < state->numPages; i++)
	{
		if (state->status[i] == pageNum && pageNum != 0)
		{
			state->bufferHits++;
			buf = state->buffer + state->pageSize*i;
			state->lastHit = state->status[i];
			return buf;
		}
	}

	if (state->numPages == 2)
	{	buf = state->buffer + state->pageSize;
		i = 1;
	}
	else
	{	
		/* Reserve page #1 for root if have at least 3 buffers. */
		if (state->activePath[0] == pageNum)
		{	/* Request for root. */			
			i = 1;
		}
		else
		{
			if (state->numPages == 3)
			{	/* With 3 pages and not the root, always reusing the 3rd buffer for reading. */
				buf = state->buffer + state->pageSize*2;
				i = 2;
			}
			else
			{
				/* More than minimum pages. Some basic memory management using round robin buffer. */		
				buf = NULL;
				/* Determine buffer location for page */
				/* TODO: This needs to be improved and may also consider locking pages */
				for (i=2; i < state->numPages; i++)
				{
					if (state->status[i] == 0)	/* Empty page */
					{	buf = state->buffer + state->pageSize*i;			
						break;
					}
				}

				/* Pick the next page */
				if (buf == NULL)
				{
					i = state->nextBufferPage;
					state->nextBufferPage++;
					while (1)
					{
						if (i > state->numPages-1)
						{	i = 2;
							state->nextBufferPage = 2;
						}

						if (state->status[i] != state->lastHit)						
							break;					

						i++;
					}		
				}
			}
		}
	}
	    
	state->status[i] = pageNum;
	return readPageBuffer(state, pageNum, i);
}

/**
@brief      Reads page to a particular buffer number. Returns pointer to buffer if success.
@param     	state
                DBbuffer state structure
@param     	pageNum
                Physical page id (number)
@param		bufferNum
				Buffer to read into
@return		Returns pointer to buffer page or NULL if error.
*/
void* readPageBuffer(dbbuffer *state, id_t pageNum, count_t bufferNum)
{
	void *buf = state->buffer + bufferNum * state->pageSize;	
	FILE* fp = state->file;
  
    /* Seek to page location in file */
    fseek(fp, pageNum*state->pageSize, SEEK_SET);

    /* Read page into start of buffer 1 */   
    if (0 ==  fread(buf, state->pageSize, 1, fp))
    	return NULL;       
    
    state->numReads++;
	   
	return buf;
}

/**
@brief      Erases physical pages start to end inclusive. Assumes that start and end are aligned according to erase block.
@param     	state
               	DBbuffer state structure
@param     	startPage
                Physical index of start page
@param     	endPage
				Physical index of start page
@return		Return 0 if success, -1 if failure.
*/
int8_t erasePages(dbbuffer *state, id_t startPage, id_t endPage)
{
	// printf("Erasing pages. Start: %d  End: %d\n", startPage, endPage);
	/* TODO: Not doing anything for file. */
}

/**
@brief      Writes page to storage. Returns physical page id if success. -1 if failure.
			This version does not check for wrap around.
@param     	state
               	DBbuffer state structure
@param     	buffer
                In memory buffer containing page
@param		pageNum
				Location to write at
@return		
*/
int32_t writeBytes(dbbuffer *state, void* buffer, count_t size, int32_t pageNum, int32_t offset)
{			
	/* Seek to page location in file */
    fseek(state->file, pageNum*state->pageSize+offset, SEEK_SET);

	fwrite(buffer, size, 1, state->file);
	#ifdef DEBUG_WRITE
            printf("Wrote block. Idx: %d Cnt: %d\n", *((int32_t*) buffer), SBTREE_GET_COUNT(state->buffer));
			printf("BM: "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY( *((uint8_t*) (state->buffer+state->bmOffset))));
            for (int k = 0; k < SBTREE_GET_COUNT(buffer); k++)
            {
                test_record_t *buf = (void *)(buffer + state->headerSize + k * state->recordSize);
                printf("%d: Output Record: %d\n", k, buf->key);
            }
	#endif

	// state->numWrites++;		
	
	// printf("\nWrite bytes page: %d Id: %d Key: %d\n", pageNum, (state->nextPageId-1), *((int32_t*) (buffer+10)));
	return pageNum;
}

/**
@brief      Writes page to storage. Returns physical page id if success. -1 if failure.
			This version does not check for wrap around.
@param     	state
               	DBbuffer state structure
@param     	buffer
                In memory buffer containing page
@param		pageNum
				Location to write at
@return		
*/
int32_t writePageDirect(dbbuffer *state, void* buffer, int32_t pageNum)
{
	/* Setup page number in header */	
	memcpy(buffer, &(state->nextPageId), sizeof(id_t));
	state->nextPageId++;
		
	/* Seek to page location in file */
    fseek(state->file, pageNum*state->pageSize, SEEK_SET);

	fwrite(buffer, state->pageSize, 1, state->file);
	#ifdef DEBUG_WRITE
            printf("Wrote block. Idx: %d Cnt: %d\n", *((int32_t*) buffer), SBTREE_GET_COUNT(state->buffer));
			printf("BM: "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY( *((uint8_t*) (state->buffer+state->bmOffset))));
            for (int k = 0; k < SBTREE_GET_COUNT(buffer); k++)
            {
                test_record_t *buf = (void *)(buffer + state->headerSize + k * state->recordSize);
                printf("%d: Output Record: %d\n", k, buf->key);
            }
	#endif

	state->numWrites++;		
	
	// printf("\nWrite page: %d Id: %d Key: %d\n", pageNum, (state->nextPageId-1), *((int32_t*) (buffer+10)));
	return pageNum;
}

/**
@brief      Overwrites page to storage at same physical address. -1 if failure.
			Caller is responsible for knowing that overwrite is possible given page contents.
@param     	state
                DBbuffer state structure
@param     	buffer
                In memory buffer containing page
@return		
*/
int32_t overWritePage(dbbuffer *state, void* buffer, int32_t pageNum)
{			
	/* Seek to page location in file */
    fseek(state->file, pageNum*state->pageSize, SEEK_SET);

	fwrite(buffer, state->pageSize, 1, state->file);
	#ifdef DEBUG_WRITE
            printf("Wrote block. Idx: %d Cnt: %d\n", *((int32_t*) buffer), SBTREE_GET_COUNT(state->buffer));
			printf("BM: "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY( *((uint8_t*) (state->buffer+state->bmOffset))));
            for (int k = 0; k < SBTREE_GET_COUNT(buffer); k++)
            {
                test_record_t *buf = (void *)(buffer + state->headerSize + k * state->recordSize);
                printf("%d: Output Record: %d\n", k, buf->key);
            }
	#endif

	state->numOverWrites++;		
	
	/* Check if buffer contains this page */
	for (count_t i=1; i < state->numPages; i++)
	{		
		if (state->status[i] == pageNum && pageNum != 0)
		{	/* Copy over page */
			memcpy(state->buffer + i*state->pageSize, buffer, state->pageSize);
			// Other choice is to clear the buffer
			// state->status[i] = 0;			
		}
	}

	// printf("\nWrite page: %d Id: %d Key: %d\n", pageNum, (state->nextPageId-1), *((int32_t*) (buffer+10)));
	return pageNum;
}

/**
@brief      Writes page to storage. Returns physical page id if success. -1 if failure.
@param     	state
               	DBbuffer state structure
@param     	buffer
                In memory buffer containing page
@return		
*/
int32_t writePage(dbbuffer *state, void* buffer)
{    		
	int32_t pageNum;

	/* Determine physical page location. */
	while (state->nextPageWriteId == state->blockEndPage)
	{		
		id_t startErase, endErase;

		/* At end of block */
		/* Advance end of erasedEndPage */
		if (state->blockEndPage >= state->endDataPage)
		{	// state->blockEndPage = -1;
			state->nextPageWriteId = 0;
		}
		else
		{
			state->nextPageWriteId = state->erasedStartPage;
		}
		state->blockEndPage = state->erasedStartPage + state->eraseSizeInPages - 1;
		
finderase:
		
		state->erasedStartPage += state->eraseSizeInPages;
		startErase = state->erasedStartPage-state->eraseSizeInPages+state->eraseSizeInPages;
		endErase = state->erasedStartPage+state->eraseSizeInPages-1;
		
		
		if (endErase > state->endDataPage)
		{	/* At end of memory region. Must wrap around */
			state->wrappedMemory = 1;

			startErase = 0;
			endErase = state->eraseSizeInPages-1;
			state->erasedStartPage = 0;
		}		

		if (state->wrappedMemory != 0)
		{	/* Have went through memory at least once. Whatever is erased is actual data that is no longer available. */
			// printf("Must move pages between: %d and %d\n", startErase, endErase);
			if (startErase == 20)
				printf("a\n");
			/* TODO: Check for valid pages just beyond erasedEndPage and copy to the currently erased block if necessary */
			id_t parentId, newIdx;
			void *parentBuffer;
			count_t numMove = 0;
			int32_t pageIdToMove[10];		/* TODO: Should be size of erase size */
			id_t parentIdToMove[10];

			for (id_t i=startErase; i <= endErase; i++)
			{
				int8_t response = state->isValid(state->state, i, &parentId, &parentBuffer);
				// printf("Status page: %d  Status: %d\n", i, response);
				if (response == -1)
					continue;

				if (response == 1)
				{	/* Mapping but no valid node. Must update parent but not the node itself. */
					pageIdToMove[numMove] = -1;
				}
				else
				{	/* Valid node at this location. Must rewrite node and its parent. */
					pageIdToMove[numMove] = i;
				}
				
				parentIdToMove[numMove] = parentId;
				numMove++;
			}

			if (numMove >= state->eraseSizeInPages/2)
			{
				// printf("Skipping pages and leaving as is. Start: %d End: %d\n", startErase, endErase);				
				// state->erasedStartPage += state->eraseSizeInPages;
				goto finderase;
				/* TODO: How to skip over full blocks */
			}

			for (id_t i=0; i < numMove; i++)
			{			
				if (pageIdToMove[i] != -1)
				{	/* Must move page */
					// printf("Moving page: %d\n", pageIdToMove[i]);
				
					/* Read page */
					void *buf = readPage(state, pageIdToMove[i]);
					if (buf != NULL)
					{
						pageNum = state->nextPageWriteId++;

						/* Update page */
						state->movePage(state->state, pageIdToMove[i], pageNum, buf);

						/* Write page */
						writePageDirect(state, buf, pageNum);

						btreePrintNodeBuffer(state->state, pageNum, 0, buf);
					}
				}
				
				/* Write parent so does not have mapping to previous page */
				/* Read parent page */
				parentId = parentIdToMove[i];
				if (btreeGetMapping(state->state, parentId) != parentId)
				{	/* Node has already been moved. Do not move it. Remove mapping. */
					printf("Skipping parent page: %d\n", parentId);
					btreeDeleteMapping(state->state, parentId);
					continue;
				}
				printf("Writing parent page: %d\n", parentId);
				if (parentId == 37)
					printf("HERE\n");
				void *bufParent = readPage(state, parentId);
				if (bufParent != NULL)
				{
					pageNum = state->nextPageWriteId++;
					
					/* Update page */
					id_t prevId = btreeUpdatePrev(state->state, bufParent, parentId);
					state->movePage(state->state, parentId, pageNum, bufParent);	

					/* Update page */
					// state->movePage(state->state, i, pageNum, bufParent);

					/* Write page */
					writePageDirect(state, bufParent, pageNum);

					btreePrintNodeBuffer(state->state, pageNum, 0, bufParent);

					btreePrintMappings(state->state);
				}															
			}
		}
		
		/* Erase the next erase block */
		erasePages(state, startErase, endErase);
		// erasePages(state, state->erasedEndPage-state->eraseSizeInPages+1+state->eraseSizeInPages, state->erasedEndPage+state->eraseSizeInPages);
	}
	
	pageNum = state->nextPageWriteId++;
	// vmtreePrintNodeBuffer(state->state, pageNum, 0, buffer);
	return writePageDirect(state, buffer, pageNum);	
}


/**
@brief     	Initialize in-memory buffer page.
@param     	state
                DBbuffer state structure
@param     	pageNum
                In memory buffer page id (number)
@return		pointer to initialized page
*/
void* initBufferPage(dbbuffer *state, int pageNum)
{	
	/* Insure all values are 0 in page. */
	/* TODO: May want to initialize to all 1s for certian memory types. */	
	void *buf = state->buffer + pageNum * state->pageSize;
	for (uint16_t i = 0; i < state->pageSize/sizeof(int32_t); i++)
    {
        ((int32_t*) buf)[i] = 0;
    }
	
	return buf;		
}

/**
@brief     	Closes buffer.
@param     	state
                DBbuffer state structure
*/
void closeBuffer(dbbuffer *state)
{
	printStats(state);	
	fclose(state->file);
}

/**
@brief     	Prints statistics.
@param     	state
                DBbuffer state structure
*/
void printStats(dbbuffer *state)
{
	printf("Num reads: %d\n", state->numReads);
	printf("Buffer hits: %d\n", state->bufferHits);
	printf("Num writes: %d\n", state->numWrites);
	printf("Num overwrites: %d\n", state->numOverWrites);
}