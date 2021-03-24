/******************************************************************************/
/**
@file		test_btree.c
@author		Ramon Lawrence
@brief		This file does performance/correctness testing of BTree.
@copyright	Copyright 2021
			The University of British Columbia,
			Ramon Lawrence
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
#include <time.h>
#include <string.h>

#include "btree.h"
#include "randomseq.h"


void testIterator(btreeState *state, void *recordBuffer)
{
     /* Below minimum key search */
    int32_t key = -1;
    int8_t result = btreeGet(state, &key, recordBuffer);
    if (result == 0) 
        printf("Error1: Key found: %d\n", key);

    /* Above maximum key search */
    key = 3500000;
    result = btreeGet(state, &key, recordBuffer);
    if (result == 0) 
        printf("Error2: Key found: %d\n", key);
    
    free(recordBuffer);
    
    btreeIterator it;
    uint32_t mv = 40;     // For all records, select mv = 1.
    it.minKey = &mv;
    int v = 299;
    it.maxKey = &v;       

    // btreePrint(state); 

    btreeInitIterator(state, &it);
    uint32_t i = 0;
    int8_t success = 1;    
    uint32_t *itKey, *itData;

    while (btreeNext(state, &it, (void**) &itKey, (void**) &itData))
    {                      
        // printf("Key: %d  Data: %d\n", *itKey, *itData);
        if (i+mv != *itKey)
        {   success = 0;
            printf("Key: %d Error\n", *itKey);
        }
        i++;        
    }
    printf("\nRead records: %d\n", i);

    if (success && i == (v-mv+1))
        printf("SUCCESS\n");
    else
        printf("FAILURE\n");           
}

void testRecovery()
{
    srand(3);
    randomseqState rnd;
    rnd.size = 1000;
    uint32_t n = rnd.size; 
    rnd.prime = 0;
    uint32_t end, i, errors = 0;

    /* Configure buffer */
    dbbuffer* buffer = (dbbuffer*) malloc(sizeof(dbbuffer));
    if (buffer == NULL)
    {   printf("Failed to allocate buffer struct.\n");
        return;
    }
    buffer->pageSize = 512;
    buffer->numPages = 3;
    buffer->status = (id_t*) malloc(sizeof(id_t)*3);
    if (buffer->status == NULL)
    {   printf("Failed to allocate buffer status array.\n");
        return;
    }
        
    buffer->buffer  = malloc((size_t) buffer->numPages * buffer->pageSize);   
    if (buffer->buffer == NULL)
    {   printf("Failed to allocate buffer.\n");
        return;
    }
    /* Configure btree state */
    btreeState* state = (btreeState*) malloc(sizeof(btreeState));
    if (state == NULL)
    {   printf("Failed to B-tree state struct.\n");
        return;
    }
    state->recordSize = 16;
    state->keySize = 4;
    state->dataSize = 12;       

    /* Connections between buffer and btree */
    buffer->activePath = state->activePath;
    buffer->state = state;    

    state->tempKey = malloc(sizeof(int32_t)); 
    state->tempData = malloc(12); 
    int8_t* recordBuffer = (int8_t*) malloc(state->recordSize);    	

    /* Setup output file. File must exist. */
    FILE *fp;
    fp = fopen("myfile.bin", "r+b");
    if (NULL == fp) {
        printf("Error: Can't open file!\n");
        return;
    }
    
    buffer->file = fp;

    state->parameters = 0;    
    state->buffer = buffer;

    /* Initialize btree structure with parameters */
    btreeRecover(state);
        
    /* Data record is empty. Only need to reset to 0 once as reusing struct. */        
    for (i = 0; i < (uint16_t) (state->recordSize-4); i++) // 4 is the size of the key
    {
        recordBuffer[i + sizeof(int32_t)] = 0;
    }

    unsigned long start = clock();   

    
    printf("\nVerifying and searching for all values.\n");
    start = clock();

    srand(3);
    randomseqInit(&rnd);

    /* Verify that can find all values inserted */    
    for (i = 1; i <= n; i++) 
    { 
        int32_t key = randomseqNext(&rnd);
        int8_t result = btreeGet(state, &key, recordBuffer);
        if (result != 0) 
        {   errors++;
            printf("ERROR: Failed to find: %d\n", key);
            btreeGet(state, &key, recordBuffer);
        }
        else if (*((int32_t*) recordBuffer) != key)
        {   printf("ERROR: Wrong data for: %d\n", key);
            printf("Key: %d Data: %d\n", key, *((int32_t*) recordBuffer));
        }       
    }

    if (errors > 0)
        printf("FAILURE: Errors: %d\n", errors);
    else
        printf("SUCCESS. All values found!\n");
    
    end = clock();
    printf("Elapsed Time: %lu s\n", (end - start));
    printf("Records queried: %d\n", n);   
    printStats(state->buffer);     

    closeBuffer(buffer);    
        
    free(state->tempKey);
    free(state->tempData);
    free(recordBuffer);

    free(buffer->status);
    free(state->buffer->buffer);
    free(buffer);
    free(state);
}


/**
 * Runs all tests and collects benchmarks
 */ 
void runalltests_btree()
{    
    uint32_t stepSize = 10000, numSteps = 10;
    count_t r, numRuns = 3, l;
    uint32_t times[numSteps][numRuns];
    uint32_t reads[numSteps][numRuns];
    uint32_t writes[numSteps][numRuns];
    uint32_t overwrites[numSteps][numRuns];
    uint32_t hits[numSteps][numRuns];    
    uint32_t rtimes[numSteps][numRuns];
    uint32_t rreads[numSteps][numRuns];
    uint32_t rhits[numSteps][numRuns];

    int8_t M = 2;  

    /* Optional: Test recovery. Must run main test first to generate B-tree file. */
    // testRecovery();
    // return;

    for (r=0; r < numRuns; r++)
    {
        uint32_t errors = 0;
        uint32_t i;

        srand(r);
        randomseqState rnd;
        rnd.size = 100000;
        uint32_t n = rnd.size; 
        rnd.prime = 0;
    
        /* Configure buffer */
        dbbuffer* buffer = (dbbuffer*) malloc(sizeof(dbbuffer));
        if (buffer == NULL)
        {   printf("Failed to allocate buffer struct.\n");
            return;
        }
        buffer->pageSize = 512;
        buffer->numPages = M;
        buffer->status = (id_t*) malloc(sizeof(id_t)*M);
        if (buffer->status == NULL)
        {   printf("Failed to allocate buffer status array.\n");
            return;
        }
        
        buffer->buffer  = malloc((size_t) buffer->numPages * buffer->pageSize);   
        if (buffer->buffer == NULL)
        {   printf("Failed to allocate buffer.\n");
            return;
        }
        /* Configure btree state */
        btreeState* state = (btreeState*) malloc(sizeof(btreeState));
        if (state == NULL)
        {   printf("Failed to B-tree state struct.\n");
            return;
        }
        state->recordSize = 16;
        state->keySize = 4;
        state->dataSize = 12;       

        /* Connections between buffer and btree */
        buffer->activePath = state->activePath;
        buffer->state = state;    

        state->tempKey = malloc(sizeof(int32_t)); 
        state->tempData = malloc(12); 
        int8_t* recordBuffer = (int8_t*) malloc(state->recordSize);    	

        /* Setup output file. */
        FILE *fp;
        fp = fopen("myfile.bin", "w+b");
        if (NULL == fp) {
            printf("Error: Can't open file!\n");
            return;
        }
        
        buffer->file = fp;

        state->parameters = 0;    
        state->buffer = buffer;

        /* Initialize btree structure with parameters */
        btreeInit(state);
        
        /* Data record is empty. Only need to reset to 0 once as reusing struct. */        
        for (i = 0; i < (uint16_t) (state->recordSize-4); i++) // 4 is the size of the key
        {
            recordBuffer[i + sizeof(int32_t)] = 0;
        }

        unsigned long start = clock();
    
        srand(r);
        randomseqInit(&rnd);
    
        
        for (i = 1; i <= n ; i++)
        {           
            id_t v = randomseqNext(&rnd);
               
            *((int32_t*) recordBuffer) = v;
            *((int32_t*) (recordBuffer+4)) = v;             

            if (btreePut(state, recordBuffer, (void*) (recordBuffer + 4)) == -1)
            {  
                btreePrint(state);               
                printf("INSERT ERROR: %d\n", v);
                return;
            }
            
            if (i % stepSize == 0)
            {           
                printf("Num: %lu KEY: %lu\n", i, v);
                // btreePrint(state);               
                l = i / stepSize -1;
                if (l < numSteps && l >= 0)
                {
                    times[l][r] = (clock()-start)*1000/CLOCKS_PER_SEC;
                    reads[l][r] = state->buffer->numReads;
                    writes[l][r] = state->buffer->numWrites;
                    overwrites[l][r] = state->buffer->numOverWrites;
                    hits[l][r] = state->buffer->bufferHits;                     
                }
            }        
        }    

        unsigned long end = clock();   

        l = numSteps-1;
        times[l][r] = (end-start)*1000/CLOCKS_PER_SEC;
        reads[l][r] = state->buffer->numReads;
        writes[l][r] = state->buffer->numWrites;
        overwrites[l][r] = state->buffer->numOverWrites;
        hits[l][r] = state->buffer->bufferHits;                              

        // btreePrint(state);             
        printStats(state->buffer);

        printf("Elapsed Time: %lu ms\n", times[l][r]);
        printf("Records inserted: %lu\n", n);

        /* Clear stats */
        btreeClearStats(state);

        printf("\nVerifying and searching for all values.\n");
        start = clock();

        srand(1);
        randomseqInit(&rnd);

        /* Verify that can find all values inserted */    
        for (i = 1; i <= n; i++) 
        { 
            int32_t key = randomseqNext(&rnd);
            int8_t result = btreeGet(state, &key, recordBuffer);
            if (result != 0) 
            {   errors++;
                printf("ERROR: Failed to find: %d\n", key);
                btreeGet(state, &key, recordBuffer);
            }
            else if (*((int32_t*) recordBuffer) != key)
            {   printf("ERROR: Wrong data for: %d\n", key);
                printf("Key: %d Data: %d\n", key, *((int32_t*) recordBuffer));
            }

            if (i % stepSize == 0)
            {                           
                // btreePrint(state);               
                l = i / stepSize - 1;
                if (l < numSteps && l >= 0)
                {
                    rtimes[l][r] = (clock()-start)*1000/CLOCKS_PER_SEC;
                    rreads[l][r] = state->buffer->numReads;                    
                    rhits[l][r] = state->buffer->bufferHits;                     
                }
            }    
        }

        l = numSteps-1;       
        rtimes[l][r] = (clock()-start)*1000/CLOCKS_PER_SEC;
        rreads[l][r] = state->buffer->numReads;                    
        rhits[l][r] = state->buffer->bufferHits;                     
    
        if (errors > 0)
            printf("FAILURE: Errors: %d\n", errors);
        else
            printf("SUCCESS. All values found!\n");
        
        end = clock();
        printf("Elapsed Time: %lu ms\n", rtimes[l][r]);
        printf("Records queried: %lu\n", n);   
        printStats(state->buffer);     

        /* Optional: Test iterator */
        // testIterator(state, recordBuffer);
        // printStats(buffer);

        /* Clean up and free memory */
        closeBuffer(buffer);    
        
        free(state->tempKey);
        free(state->tempData);
        free(recordBuffer);

        free(buffer->status);
        free(state->buffer->buffer);
        free(buffer);
        free(state);
    }

    // Prints results
    uint32_t sum;
    for (count_t i=1; i <= numSteps; i++)
    {
        printf("Stats for %lu:\n", i*stepSize);
    
        printf("Reads:   ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += reads[i-1][r];
            printf("\t%lu", reads[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("Writes: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += writes[i-1][r];
            printf("\t%lu", writes[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        

        printf("Overwrites: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += overwrites[i-1][r];
            printf("\t%lu", overwrites[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        
        printf("Totwrites: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += overwrites[i-1][r] + writes[i-1][r];
            printf("\t%lu", overwrites[i-1][r] + writes[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("Buffer hits: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += hits[i-1][r];
            printf("\t%lu", hits[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        

        printf("Write Time: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += times[i-1][r];
            printf("\t%lu", times[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        
        printf("R Time: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rtimes[i-1][r];
            printf("\t%lu", rtimes[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("R Reads: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rreads[i-1][r];
            printf("\t%lu", rreads[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("R Buffer hits: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rhits[i-1][r];
            printf("\t%lu", rhits[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
    }
}


/**
 * Main function to run tests
 */ 
void main()
{
	runalltests_btree();
}  
