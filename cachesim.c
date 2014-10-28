#include "cachesim.h"
#include <stdio.h>
#include <stdlib.h>

struct store{
	uint64_t tag;
	unsigned int valid_bit : 1;
	unsigned int dirty_bit : 1;
	unsigned int prefetch_bit : 1;
	unsigned int this_bit : 1;
};

struct victim_store{
	uint64_t tag;
	uint64_t index;
	unsigned int valid_bit : 1;
	unsigned int dirty_bit : 1;
	unsigned int prefetch_bit : 1;
};

//uint64_t LRU_stack*;

struct LRU_root{
	uint64_t **stack;
	uint64_t *elements_count;
};

struct cache{
	struct store **main_cache;		//Define a pointer to the main cache
	struct victim_store *victim_cache;		//Define a pointer to the victim cache
	struct LRU_root root;			//Define a pointer to the LRU structure
} cool_cache;


uint64_t actual_c;		//Stores the user-input c value
uint64_t actual_b;		//Stores the user-input b value
uint64_t actual_s;		//Stores the user-input s value
uint64_t actual_v;		//Stores the user-input v value
uint64_t actual_k;		//Stores the user-input k value
uint64_t max_way;				//Calculate the number of ways (or blocks/set)
struct cache_stats_t* global_p_stats;
uint64_t *temp_stack;
uint64_t temp_stack_count;
uint64_t pending_stride;
uint64_t last_miss_block_addr;
uint64_t block_offset;

void swap(uint64_t index, uint64_t hit_way, uint64_t hit_way_v);
uint64_t power(int x,uint64_t y);	//To calculate the x^y
uint64_t extract_info(uint64_t address, int choice);	//To calculate offset, tag or index from address
void send_to_victim_cache(uint64_t hit_way, uint64_t index, int flag, uint64_t hit_way1);		//To send data to victim cache
uint64_t kick_LRU(uint64_t index);		//Returns LRU way number from the LRU table and deletes that entry
void insert_into_main_cache(uint64_t hit_way, uint64_t index, uint64_t tag, int db);		//Inserts data into the main cache memory and makes it as an MRU entry in the LRU table
void prioritize(uint64_t index, uint64_t hit_way);		//Makes an entry MRU in the LRU table
uint64_t check_cache(uint64_t tag, uint64_t way, uint64_t index, char *hit_flag);		// Checks the cache for the requested item
void clean_up(struct cache_stats_t *p_stats) ;		// Performs clean up of the cache - i.e. - it checks for write backs and updates the counter accordingly
void push(uint64_t index, uint64_t hit_way, char flag, char hit_flag, uint64_t hit_way1f);
uint64_t pop(uint64_t index, char flag);
uint64_t block_address(uint64_t address);
void prefetch(uint64_t address);
void put_in_LRU(uint64_t prefetch_tag,uint64_t prefetch_index,char flag,uint64_t hit_way);
void swap_LRU(uint64_t index, uint64_t hit_way, uint64_t hit_way_v);
uint64_t kick_LRU_prefetch(uint64_t index);


void prefetch(uint64_t address){
	uint64_t i,prefetch_address,prefetch_tag,prefetch_index;//prefetch_way,prefetch_hit_way
	char prefetch_hit_flag = '0';
	uint64_t d = 0;
	
	d = block_address(address) - last_miss_block_addr;
	
	//printf("d = %"PRIx64"\n",d);
	
	if(d == pending_stride && d>0){
		int way=0;

		prefetch_address = block_address(address);
		for(i=1;i<=actual_k;i++){
			prefetch_address = prefetch_address+d;
			//printf("prefetch address = %"PRIx64"\n",prefetch_address);
			prefetch_tag = extract_info(prefetch_address,0);			//Extracts the tag value from the address
			prefetch_index = extract_info(prefetch_address,1);		//Extracts the index value from the address
			//prefetch_way = power(2,actual_s);				//Calculate the number of ways (or blocks/set)
			global_p_stats->prefetched_blocks++;
			global_p_stats->bytes_transferred += block_offset;
			prefetch_hit_flag = '0';

			for(way = 0; way<max_way; way++){	//Checks for empty/hit in the main cache
				if (cool_cache.main_cache[prefetch_index][way].valid_bit == 0){
					prefetch_hit_flag = 'E';
					break;
				
				}else if(cool_cache.main_cache[prefetch_index][way].tag == prefetch_tag){
					prefetch_hit_flag = 'C';
					break;

				}
			}
			
			if(prefetch_hit_flag == '0'){	//if main cache is full and no hits have occured
				for(way = 0; way<actual_v; way++){		//checks the victim cache
					if (cool_cache.victim_cache[way].valid_bit == 0){
						prefetch_hit_flag = 'U';
						break;
					}else if(cool_cache.victim_cache[way].tag == prefetch_tag && cool_cache.victim_cache[way].index == prefetch_index){
						prefetch_hit_flag = 'V';
						break;
					}
				}
			}
			
			put_in_LRU(prefetch_tag,prefetch_index,prefetch_hit_flag, way);	//inserts the prefetched block
		}

		prefetch_address = block_address(address);
		for(i=1;i<=actual_k;i++){
			prefetch_address = prefetch_address+d;
			prefetch_index = extract_info(prefetch_address,1);		//Extracts the index value from the address
			
			for(way = 0; way<max_way; way++){
				cool_cache.main_cache[prefetch_index][way].this_bit = 0;
			}
			
			/*for(way = 0; way<actual_v; way++){
				cool_cache.victim_cache[way].this_bit = 0;
			}*/
		}
	}
	last_miss_block_addr = block_address(address);
	pending_stride = d;
}

void put_in_LRU(uint64_t prefetch_tag,uint64_t prefetch_index,char flag,uint64_t hit_way){
	
	int count;
	//printf("prefetch tag = %"PRIx64"\n",prefetch_tag);
	//printf("prefetch index = %"PRIx64"\n",prefetch_index);
	
	if(flag == 'C'){

	}else if(flag == 'E'){
		cool_cache.main_cache[prefetch_index][hit_way].valid_bit = 1;
		cool_cache.main_cache[prefetch_index][hit_way].prefetch_bit = 1;
		cool_cache.main_cache[prefetch_index][hit_way].this_bit = 1;
		cool_cache.main_cache[prefetch_index][hit_way].dirty_bit = 0;
		cool_cache.main_cache[prefetch_index][hit_way].tag = prefetch_tag;
		
		cool_cache.root.elements_count[prefetch_index]--;
		for(count = cool_cache.root.elements_count[prefetch_index]; count <max_way-1; count++){
			cool_cache.root.stack[prefetch_index][count] = cool_cache.root.stack[prefetch_index][count+1];
		}
		cool_cache.root.stack[prefetch_index][count] = hit_way;
	//	printf("way = %"PRIx64"\n",hit_way);
	
	}else if(flag == 'V'){
		uint64_t main_way = kick_LRU_prefetch(prefetch_index);
		swap_LRU(prefetch_index,main_way,hit_way);

	}else if(flag == '0' || flag == 'U'){
		uint64_t hit_way = kick_LRU_prefetch(prefetch_index);
		send_to_victim_cache(hit_way, prefetch_index, 1, actual_v-1);
		cool_cache.main_cache[prefetch_index][hit_way].valid_bit = 1;
		cool_cache.main_cache[prefetch_index][hit_way].prefetch_bit = 1;
		cool_cache.main_cache[prefetch_index][hit_way].this_bit = 1;
		cool_cache.main_cache[prefetch_index][hit_way].dirty_bit = 0;
		cool_cache.main_cache[prefetch_index][hit_way].tag = prefetch_tag;
	//	printf("way = %"PRIx64"\n",hit_way);
		
	}

}

void swap_LRU(uint64_t index, uint64_t hit_way, uint64_t hit_way_v){
	
	struct victim_store temp_block;

	temp_block.tag = cool_cache.victim_cache[hit_way_v].tag;
	temp_block.index = cool_cache.victim_cache[hit_way_v].index;
	temp_block.valid_bit = cool_cache.victim_cache[hit_way_v].valid_bit;
	temp_block.dirty_bit = cool_cache.victim_cache[hit_way_v].dirty_bit;
	
	cool_cache.victim_cache[hit_way_v].tag = cool_cache.main_cache[index][hit_way].tag;
	cool_cache.victim_cache[hit_way_v].index = index;
	cool_cache.victim_cache[hit_way_v].valid_bit = cool_cache.main_cache[index][hit_way].valid_bit;
	cool_cache.victim_cache[hit_way_v].dirty_bit = cool_cache.main_cache[index][hit_way].dirty_bit;
	cool_cache.victim_cache[hit_way_v].prefetch_bit = cool_cache.main_cache[index][hit_way].prefetch_bit;
	
	cool_cache.main_cache[index][hit_way].tag = temp_block.tag;
	cool_cache.main_cache[index][hit_way].valid_bit = temp_block.valid_bit;
	cool_cache.main_cache[index][hit_way].dirty_bit = temp_block.dirty_bit;
	cool_cache.main_cache[index][hit_way].prefetch_bit = 1;
	cool_cache.main_cache[index][hit_way].this_bit = 1;
}


uint64_t kick_LRU_prefetch(uint64_t index){
	uint64_t way;
	int count=0;
	
	if(actual_s == 0){
		way = 0;
		return way;
	}
	
	for(count = max_way-1; count >= cool_cache.root.elements_count[index];count--){
		way = cool_cache.root.stack[index][count];
		
		if(cool_cache.main_cache[index][way].this_bit == 0){
			break;
		}
	}
	
	while(count < max_way-1){
		cool_cache.root.stack[index][count] = cool_cache.root.stack[index][count+1];
		count++;
	}
	
	cool_cache.root.stack[index][count] = way;
	
	return way;
}


uint64_t block_address(uint64_t address){
	uint64_t info, mask, temp;
	
	temp = (uint64_t) power(2,actual_b)-1;
	mask = (uint64_t) ~temp;			//Calculate the mask for finding tag value
	
	info = address & mask;			//Bitwise AND operation between the address and the mask gives us the tag
	return info;
}


void setup_cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v, uint64_t k) {
	uint64_t index,victim_index,i;
	
	actual_c = c;		//update the value of c in global variable
	actual_b = b;		//update the value of b in global variable
	actual_s = s;		//update the value of s in global variable
	actual_v = v;		//update the value of v in global variable
	actual_k = k;		//update the value of k in global variable
	
	//Implementation of store ---- start
	
	index = power(2,(c-b-s));		//Calculate the number of sets (or rows)
	max_way = power(2,s);				//Calculate the number of ways (or blocks/set)
	block_offset = power(2,b);
	
	temp_stack_count = max_way;
	temp_stack = (uint64_t *) calloc(max_way,sizeof(uint64_t));

	//Checks if input is correct
	if(index>0){			//Condition satisfied for C >= S+B => fully-associative or direct-mapped or set-associative
		cool_cache.main_cache = (struct store**) calloc (index,sizeof(struct store*));	//The main cache structure is allocated as heap memory. The cache is 2-D array
		for(i=0; i<index;i++){
			//printf("Inside point 5\n");
			cool_cache.main_cache[i] = (struct store*) calloc (max_way,sizeof(struct store));		//The main cache structure is initialized to zero. Note that in this only the Tag Store is simulated
			//printf("Inside point 6\n");
		}
	}else{	//If the input is wrong
		fprintf(stderr,"\nThe inputs for C, B and S are wrong!\n");
		exit(1);
	}
		
	if(v>0){		//Checks if victim cache is needed in this structure
		cool_cache.victim_cache = (struct victim_store*) calloc (v,sizeof(struct victim_store));
	}
	
	//Implementation of store ---- end
	
	//Implementation of LRU ---- start
	
	//The LRU structure is defined as an array of pointers to the LRU structure. The actual structure is implemented in runtime. Here each element in the array points to the LRU block for that array. The element at the end of that linked list points to the MRU block. Any new tag is inserted as MRU. However, pre-fetched blocks are entered as LRU.
	if(index>0){			//Condition satisfied for C >= S+B => fully-associative or direct-mapped or set-associative
		cool_cache.root.stack = (uint64_t**) calloc (index,sizeof(uint64_t*)); 
		cool_cache.root.elements_count = (uint64_t*) calloc (index,sizeof(uint64_t));
		for(i=0; i<index;i++){
			//printf("Inside point 5\n");
			cool_cache.root.elements_count[i] = max_way;
			cool_cache.root.stack[i] = (uint64_t*) calloc (max_way,sizeof(uint64_t));		//The main cache structure is initialized to zero. Note that in this only the Tag Store is simulated
			//printf("Inside point 6\n");
		}
	}
	
	//Implementation of LRU ---- end
	
	//Implementation of pre-fetch ---- start
	
	actual_k = k;
	
	//Implementation of pre-fetch ---- end
}

uint64_t power(int x,uint64_t y){
	uint64_t i, result = 1;
	for(i=0;i<y;i++){
		result = result * x;
	}
	return result;
}


void cache_access(char rw, uint64_t address, struct cache_stats_t* p_stats) {
	
	global_p_stats = p_stats;
	uint64_t tag, index, offset, way, hit_way;
	char hit_flag='0';
	
	global_p_stats->accesses += 1;		//Increments the count of cache accesses by 1 every time an access has occurred
	tag = extract_info(address,0);			//Extracts the tag value from the address
	index = extract_info(address,1);		//Extracts the index value from the address
	offset = extract_info(address,2);		//Extracts the offset value from the address
	way = power(2,actual_s);				//Calculate the number of ways (or blocks/set)
	
	switch (rw){
		case 'r':			//If a read operation is requested
			
			//printf("Inside read\nInside point 2\tindex = %" PRIu64 "\tway = %" PRIu64 "\n",index,way);
			global_p_stats->reads +=1;		//Increments the number of cache read by 1 every time a read operation has occurred
			hit_way = check_cache(tag, index, way, &hit_flag);
			//printf("hit flag -> %c\n", hit_flag);

			if(hit_flag == 'C'){	//If a HIT has occurred in main cache
				prioritize(index, hit_way);
				//printf("HIT\n");
				
			}else if(hit_flag=='P'){
				cool_cache.main_cache[index][hit_way].prefetch_bit = 0;
				prioritize(index, hit_way);
				p_stats->useful_prefetches++;
				//printf("HIT\n");
				
			}else if(hit_flag == 'E'){	//Main cache is empty and MISS has occurred
				p_stats->read_misses += 1;
				p_stats->read_misses_combined += 1;
				global_p_stats->bytes_transferred += block_offset;
				//prefetch(address);
				push(index, hit_way,'0',hit_flag,0);
				insert_into_main_cache(hit_way, index, tag,0);
				prefetch(address);
				
			}else if(hit_flag == 'V'){	//If a HIT has occurred in victim cache
				p_stats->read_misses += 1;
				
				//prefetch(address);
				uint64_t hit_way_m = kick_LRU(index);		//Read the LRU way number from the LRU table and delete that entry from the LRU table
				swap(index,hit_way_m,hit_way);
				
				prefetch(address);
							
			}else if(hit_flag == 'W'){	//If a HIT has occurred in victim cache
				p_stats->read_misses += 1;
				p_stats->useful_prefetches++;
				
				//prefetch(address);
				uint64_t hit_way_m = kick_LRU(index);		//Read the LRU way number from the LRU table and delete that entry from the LRU table
				swap(index,hit_way_m,hit_way);
				
				prefetch(address);
							
			}else{			//If MISS has occurred and main cache is not empty
				p_stats->read_misses += 1;
				p_stats->read_misses_combined += 1;
				global_p_stats->bytes_transferred += block_offset;
				//prefetch(address);
				hit_way = kick_LRU(index);
				push(index, hit_way,'0',hit_flag,0);
				insert_into_main_cache(hit_way, index, tag,0);
				prefetch(address);
				
			}
			
			break;
		
		case 'w':
			
			//printf("Inside write\nInside point 3\tindex = %" PRIu64 "\tway = %" PRIu64 "\n",index,way);
			global_p_stats->writes += 1;
			hit_way = check_cache(tag, index, way, &hit_flag);
			//printf("hit flag -> %c\n", hit_flag);

			if(hit_flag == 'C'){	//If a HIT has occurred in main cache
				cool_cache.main_cache[index][hit_way].dirty_bit = 1;
				prioritize(index, hit_way);
				
			}else if(hit_flag=='P'){
				cool_cache.main_cache[index][hit_way].prefetch_bit = 0;
				cool_cache.main_cache[index][hit_way].dirty_bit = 1;
				prioritize(index, hit_way);
				p_stats->useful_prefetches++;
				
			}else if(hit_flag == 'E'){	//Main cache is empty and MISS has occurred
				p_stats->write_misses += 1;
				p_stats->write_misses_combined += 1;
				global_p_stats->bytes_transferred += block_offset;
				//prefetch(address);
				push(index, hit_way,'0',hit_flag,0);
				insert_into_main_cache(hit_way, index, tag,1);
				prefetch(address);
				
			}else if(hit_flag == 'V'){	//If a HIT has occurred in victim cache
				p_stats->write_misses += 1;
				
				cool_cache.victim_cache[hit_way].prefetch_bit = 0;
				//prefetch(address);
				uint64_t hit_way_m = kick_LRU(index);		//Read the LRU way number from the LRU table and delete that entry from the LRU table
				swap(index,hit_way_m,hit_way);
				
				cool_cache.main_cache[index][hit_way_m].dirty_bit = 1;
				prefetch(address);
				
			}else if(hit_flag == 'W'){	//If a HIT has occurred in victim cache
				p_stats->write_misses += 1;
				p_stats->useful_prefetches++;
				cool_cache.victim_cache[hit_way].prefetch_bit = 0;
				
				//prefetch(address);
				uint64_t hit_way_m = kick_LRU(index);		//Read the LRU way number from the LRU table and delete that entry from the LRU table
				swap(index,hit_way_m,hit_way);
				
				cool_cache.main_cache[index][hit_way_m].dirty_bit = 1;
				prefetch(address);
							
			}else{			//If MISS has occurred and main cache is not empty
				p_stats->write_misses += 1;
				p_stats->write_misses_combined += 1;
				global_p_stats->bytes_transferred += block_offset;
				//prefetch(address);
				hit_way = kick_LRU(index);
				push(index, hit_way,'0',hit_flag,0);
				insert_into_main_cache(hit_way, index, tag,1);
				prefetch(address);
				
			}
			
			break;
		
		default:
			fprintf(stderr,"\nThe inputs for rw is wrong! ---- Undefined operation!!!\n");
			exit(2);
	}
}


//Sends data from the main cache to the victim cache
void send_to_victim_cache(uint64_t hit_way, uint64_t index, int flag, uint64_t hit_way1){
	
	uint64_t victim_index = hit_way1;				//Calculate the number of blocks for victim cache
	
	if(cool_cache.main_cache[index][hit_way].valid_bit == 0){
		return;
	}
	
	if(victim_index >= 0 && actual_v > 1 && cool_cache.main_cache[index][hit_way].valid_bit==1){	//Checks if victim cache is implemented
		
//Checks if MISS has occured even in victim cache AND the last entry in victim cache is valid AND the dirty bit for the last entry in the victim cache is set
		if(flag ==1 && cool_cache.victim_cache[victim_index].valid_bit == 1 && cool_cache.victim_cache[victim_index].dirty_bit == 1){
			//write_to_memory(victim_index);
			global_p_stats->write_backs += 1;
			global_p_stats->bytes_transferred += block_offset;
		}
	
		//Implements the FIFO policy
		do{
			cool_cache.victim_cache[victim_index].index=cool_cache.victim_cache[victim_index-1].index;
			cool_cache.victim_cache[victim_index].tag=cool_cache.victim_cache[victim_index-1].tag;
			cool_cache.victim_cache[victim_index].valid_bit=cool_cache.victim_cache[victim_index-1].valid_bit;
			cool_cache.victim_cache[victim_index].dirty_bit=cool_cache.victim_cache[victim_index-1].dirty_bit;
			cool_cache.victim_cache[victim_index].prefetch_bit=cool_cache.victim_cache[victim_index-1].prefetch_bit;
			victim_index--;
		}while(victim_index>0);
	
		//Enters the data into the victim cache
		cool_cache.victim_cache[victim_index].index = index;
		cool_cache.victim_cache[victim_index].tag = cool_cache.main_cache[index][hit_way].tag;
		cool_cache.victim_cache[victim_index].valid_bit = cool_cache.main_cache[index][hit_way].valid_bit;
		cool_cache.victim_cache[victim_index].dirty_bit = cool_cache.main_cache[index][hit_way].dirty_bit;
		cool_cache.victim_cache[victim_index].prefetch_bit = cool_cache.main_cache[index][hit_way].prefetch_bit;

	}else if(victim_index == 0 && actual_v == 1 && cool_cache.main_cache[index][hit_way].valid_bit==1){
	
		//Checks if MISS has occured even in victim cache AND the last entry in victim cache is valid AND the dirty bit for the last entry in the victim cache is set
		if(flag ==1 && cool_cache.victim_cache->valid_bit == 1 && cool_cache.victim_cache->dirty_bit == 1){
			global_p_stats->write_backs += 1;
			global_p_stats->bytes_transferred += block_offset;
		}
	
		cool_cache.victim_cache->index = index;
		cool_cache.victim_cache->tag = cool_cache.main_cache[index][hit_way].tag;
		cool_cache.victim_cache->valid_bit = cool_cache.main_cache[index][hit_way].valid_bit;
		cool_cache.victim_cache->dirty_bit = cool_cache.main_cache[index][hit_way].dirty_bit;
		cool_cache.victim_cache->prefetch_bit = cool_cache.main_cache[index][hit_way].prefetch_bit;
	
	}else{
		if(flag ==1 && cool_cache.main_cache[index][hit_way].valid_bit == 1 && cool_cache.main_cache[index][hit_way].dirty_bit == 1){
			global_p_stats->write_backs += 1;
			global_p_stats->bytes_transferred += block_offset;
		}
	}
}

//Returns the LRU way number from the LRU table and deletes that entry
uint64_t kick_LRU(uint64_t index){
	uint64_t way, data;
	
	while(cool_cache.root.elements_count[index] < max_way){
		data = pop(index,'0');
		push(0,data,'1','0',0);
	}
	
	way = data;

	while(temp_stack_count < max_way){
		data = pop(index,'1');
		push(index,data,'0','0',0);
	}
	
	return way;
}

//Inserts data into the main cache memory and makes it as an MRU entry in the LRU table
void insert_into_main_cache(uint64_t hit_way, uint64_t index, uint64_t tag, int db){
	
	//printf("\n\nInside inset into main cache\n\n");
//	push(index, hit_way,'0');
	cool_cache.main_cache[index][hit_way].tag = tag;
	cool_cache.main_cache[index][hit_way].valid_bit = 1;
	if(db==2){
		cool_cache.main_cache[index][hit_way].dirty_bit = 0;
		cool_cache.main_cache[index][hit_way].prefetch_bit = 1;
	}else{
		cool_cache.main_cache[index][hit_way].dirty_bit = db;
		cool_cache.main_cache[index][hit_way].prefetch_bit = 0;
	}
	
}

void swap(uint64_t index, uint64_t hit_way, uint64_t hit_way_v){
	
	struct victim_store temp_block;
	uint64_t data = 0;
	temp_block.tag = cool_cache.victim_cache[hit_way_v].tag;
	temp_block.index = cool_cache.victim_cache[hit_way_v].index;
	temp_block.valid_bit = cool_cache.victim_cache[hit_way_v].valid_bit;
	temp_block.dirty_bit = cool_cache.victim_cache[hit_way_v].dirty_bit;
	
	cool_cache.victim_cache[hit_way_v].tag = cool_cache.main_cache[index][hit_way].tag;
	cool_cache.victim_cache[hit_way_v].index = index;
	cool_cache.victim_cache[hit_way_v].valid_bit = cool_cache.main_cache[index][hit_way].valid_bit;
	cool_cache.victim_cache[hit_way_v].dirty_bit = cool_cache.main_cache[index][hit_way].dirty_bit;
	cool_cache.victim_cache[hit_way_v].prefetch_bit = cool_cache.main_cache[index][hit_way].prefetch_bit;

	cool_cache.main_cache[index][hit_way].tag = temp_block.tag;
	cool_cache.main_cache[index][hit_way].valid_bit = temp_block.valid_bit;
	cool_cache.main_cache[index][hit_way].dirty_bit = temp_block.dirty_bit;
	cool_cache.main_cache[index][hit_way].prefetch_bit = 0;

	while(cool_cache.root.elements_count[index]<max_way-1){
		data = pop(index,'0');
		push(0,data,'1','0',0);
	}
	
	pop(index,'0');

	while(temp_stack_count<max_way){
		data = pop(index,'1');
		push(index,data,'0','0',0);
	}
	push(index,hit_way,'0','0',0);
	
}

void push(uint64_t index, uint64_t hit_way, char flag, char hit_flag, uint64_t hit_way1){
	uint64_t i, data;
	//printf("\nindex= %"PRIx64"\thit_way= %"PRIx64"\tflag= %c\n",index,hit_way,flag);
	switch (flag){
		case '0':
			if(cool_cache.root.elements_count[index]>0){
				cool_cache.root.elements_count[index]--;
				//printf("\nelements count= %"PRIx64"\n",cool_cache.root.elements_count[index]);
				cool_cache.root.stack[index][cool_cache.root.elements_count[index]] = hit_way;
			}else{
				while(cool_cache.root.elements_count[index]<max_way-1){
					data = pop(index,'0');
					push(0,data,'1','0',0);
				}
				data = pop(index,'0');
				//if(hit_flag == 'V')
				//	send_to_victim_cache(data, index, 2, hit_way1);
				//else
					send_to_victim_cache(data, index, 1, actual_v-1);

				while(temp_stack_count<max_way){
					data = pop(index,'1');
					push(index,data,'0','0',0);
				}
				push(index,hit_way,'0','0',0);
				
			}

			break;
		default:
			if(temp_stack_count>0){
				temp_stack_count--;
				temp_stack[temp_stack_count] = hit_way;
			}
	}
}

uint64_t pop(uint64_t index, char flag){
	//uint64_t i;
	uint64_t data=0;
	switch (flag){
		case '0':
			if(cool_cache.root.elements_count[index]<max_way){
				data = cool_cache.root.stack[index][cool_cache.root.elements_count[index]];
				cool_cache.root.elements_count[index]++;
			}
			break;
		default:
			if(temp_stack_count<max_way){
				data = temp_stack[temp_stack_count];
				temp_stack_count++;
			}
	}
	return data;
}

//Makes an entry MRU in the LRU table
void prioritize(uint64_t index, uint64_t hit_way){
	uint64_t data, prior;
	
	while(cool_cache.root.elements_count[index] < max_way){
		data = pop(index,'0');
		if(data == hit_way){
			break;
		}
		push(0,data,'1','0',0);
	}
	
	prior = data;
	
	while(temp_stack_count < max_way){
		data = pop(index,'1');
		push(index,data,'0','0',0);
	}
	push(index,prior,'0','0',0);
}

// Checks the cache for the requested item
uint64_t check_cache(uint64_t tag, uint64_t index, uint64_t way, char *hit_flag){

	uint64_t i;
	uint64_t victim_index = actual_v;				//Calculate the number of blocks for victim cache
	*hit_flag = '0';
	
	
	for(i=0; i<way; i++){			//checks all the possible ways in the set for the particular flag
		//checks in main cache
		if(cool_cache.main_cache[index][i].valid_bit == 1){		//checks if there is a valid bit
			if(cool_cache.main_cache[index][i].prefetch_bit == 0){
				if(cool_cache.main_cache[index][i].tag == tag){		//checks the tag value
					*hit_flag = 'C';			//on HIT, sets the HIT FLAG
					break;					//on HIT, breaks the for loop
				}
			}else{
				if(cool_cache.main_cache[index][i].tag == tag){		//checks the tag value
					*hit_flag = 'P';			//on HIT, sets the HIT FLAG
					break;					//on HIT, breaks the for loop
				}
			}
		}else{
			*hit_flag = 'E';			//Empty cache flag is set
			break;						//breaks out of the for loop
		}
	}
	
	//checks in victim cache
	if(*hit_flag == '0'){
		for (i=0;i<victim_index;i++){
			if(cool_cache.victim_cache[i].valid_bit == 1){
				if(cool_cache.victim_cache[i].prefetch_bit == 0){		//checks if there is a valid bit
					if(cool_cache.victim_cache[i].tag == tag && cool_cache.victim_cache[i].index == index){		//checks the tag value
						*hit_flag = 'V';			//on HIT, sets the HIT FLAG
						break;					//on HIT, breaks the for loop
					}
				}else{
					if(cool_cache.victim_cache[i].tag == tag && cool_cache.victim_cache[i].index == index){		//checks the tag value
						*hit_flag = 'W';			//on HIT, sets the HIT FLAG
						break;					//on HIT, breaks the for loop
					}
				}
			}else{							//if an empty position is reached without a HIT
				break;						//breaks out of the for loop
			}
		}
	
	}
	return i;
}


//The function to find offset, tag or index
uint64_t extract_info(uint64_t address, int choice){
	uint64_t info, mask, exp, temp,i;
	
	switch (choice){		//Calculate the exponent for mask calculation
		case 0: exp = actual_c - actual_s;
			temp = (uint64_t) power(2,exp)-1;
			mask = (uint64_t) ~temp;			//Calculate the mask for finding tag value
			info = address & mask;			//Bitwise AND operation between the address and the mask gives us the tag
			info = info/ power(2,actual_c-actual_s);
			break;
		case 1: exp = actual_b;
			temp = (uint64_t) power(2,exp)-1;
			mask = (uint64_t) ~temp;			//Calculate the mask for finding index value
			exp = actual_c - actual_s;
			temp = (uint64_t) power(2,exp)-1;
			mask = (uint64_t) mask & temp;
			info = address & mask;			//Bitwise AND operation between the address and the mask gives us the index
			info = info/ power(2,actual_b);
			break;
		default: exp = actual_b;
			temp = (uint64_t) power(2,exp)-1;
			mask = (uint64_t) temp;			//Calculate the mask for finding block size value
			info = address & mask;		//Bitwise AND operation between the address and the mask gives us the block size
	
	}
	
	return info;
}


void complete_cache(struct cache_stats_t *p_stats) {
	
	double s = (double)actual_s;
	
	p_stats->misses = p_stats->write_misses + p_stats->read_misses;
	p_stats->vc_misses = p_stats->write_misses_combined + p_stats->read_misses_combined;
	
	p_stats->hit_time = (double) 2.0 + 0.2*s;
	p_stats->miss_rate = (double) p_stats->misses/ (double) p_stats->accesses;
	
	p_stats->miss_penalty = 200;

	if(actual_v>0){
		p_stats->avg_access_time = (double) p_stats->hit_time + p_stats->miss_penalty*((double)p_stats->vc_misses/(double)p_stats->accesses);
	}else{
		p_stats->avg_access_time = (double) p_stats->hit_time + p_stats->miss_penalty*((double)p_stats->miss_rate);
	}
}


