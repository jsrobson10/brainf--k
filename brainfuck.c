
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_SIZE 1024

typedef struct chunk chunk;
typedef struct instruction instruction;
typedef struct instruction_node instruction_node;

typedef enum OP OP;
typedef enum OP_TYPE OP_TYPE;

enum OP
{
	OP_BOPEN, OP_BCLOSE,
	OP_SHIFT_R, OP_SHIFT_L,
	OP_INC, OP_GET, OP_PUT,
	OP_STOP
};

enum OP_TYPE
{
	OP_TYPE_OTHER, OP_TYPE_POS, OP_TYPE_VAL
};

struct chunk
{
	chunk* prev;
	chunk* next;
	char data[CHUNK_SIZE];
};

struct instruction
{
	OP type;
	long line;
	long val;
};

struct instruction_node
{
	instruction* inst;
	instruction_node* next;
};

const char DATA_EMPTY[CHUNK_SIZE] = {(char)0};

instruction* code;
chunk* cur;

long chunk_pos;

void chunk_init(chunk** c, chunk* prev, chunk* next)
{
	*c = (chunk*)calloc(1, sizeof(chunk));

	(*c)->prev = prev;
	(*c)->next = next;
}

void instruction_new(instruction** i, OP type)
{
	*i = (instruction*)malloc(sizeof(instruction));
	(*i)->type = type;
}

void instruction_node_init(instruction_node** node, instruction_node* next, instruction* inst)
{
	*node = (instruction_node*)malloc(sizeof(instruction_node));
	(*node)->next = next;
	(*node)->inst = inst;
}

void read_file(const char* filename)
{
	FILE* fp = fopen(filename, "r");
	char c = (char)1;

	instruction_node* code_start = NULL;
	instruction_node** code_end_ptr = &code_start;
	instruction_node* code_stack = NULL;
	instruction* code_last = NULL;

	OP_TYPE type = OP_TYPE_OTHER;
	long pos = 0;

	for(;;)
	{
		fread(&c, sizeof(c), 1, fp);

		if(feof(fp))
		{
			break;
		}

		long val = 0;
		instruction_node* tmp;

		switch(c)
		{
			case '>':
				if(type == OP_TYPE_POS) {code_last->val += 1; continue;}
				instruction_new(&code_last, OP_SHIFT_R);
				type = OP_TYPE_POS;
				val = 1;
				break;
			case '<':
				if(type == OP_TYPE_POS) {code_last->val -= 1; continue;}
				instruction_new(&code_last, OP_SHIFT_R);
				type = OP_TYPE_POS;
				val = -1;
				break;
			case '+':
				if(type == OP_TYPE_VAL) {code_last->val += 1; continue;}
				instruction_new(&code_last, OP_INC);
				type = OP_TYPE_VAL;
				val = 1;
				break;
			case '-':
				if(type == OP_TYPE_VAL) {code_last->val -= 1; continue;}
				instruction_new(&code_last, OP_INC);
				type = OP_TYPE_VAL;
				val = -1;
				break;
			case '[':
				instruction_new(&code_last, OP_BOPEN);
				instruction_node_init(&code_stack, code_stack, code_last);
				type = OP_TYPE_OTHER;
				break;
			case ']':
				instruction_new(&code_last, OP_BCLOSE);
				type = OP_TYPE_OTHER;

				tmp = code_stack;
				code_stack = tmp->next;
				
				val = pos - tmp->inst->line;
				tmp->inst->val = val;

				free(tmp);
				break;

			case '.':
				instruction_new(&code_last, OP_PUT);
				type = OP_TYPE_OTHER;
				break;
			case ',':
				instruction_new(&code_last, OP_GET);
				type = OP_TYPE_OTHER;
				break;
			default:
				continue;
		}

		// eof
		if(c == '\0')
		{
			break;
		}

		code_last->line = pos;
		code_last->val = val;

		instruction_node_init(code_end_ptr, NULL, code_last);
		code_end_ptr = &(*code_end_ptr)->next;

		pos++;
	}

	// add a final close instruction to the program
	instruction_new(&code_last, OP_STOP);
	instruction_node_init(code_end_ptr, NULL, code_last);

	code_last->line = pos;
	code_last->val = 0;
	pos += 1;

	// check if the program is valid
	if(code_stack != NULL)
	{
		fprintf(stderr, "Fatal: Program doesn't have enough closing brackets.\n\n");
		exit(1);
	}

	instruction_node* cur = code_start;
	instruction_node* next;

	code = (instruction*)malloc(sizeof(instruction) * pos);

	// copy all the instructions over to an array for fast access, while freeing the old linked list, while
	// converting all SHIFT_R's with negative values to SHIFT_L's because it's faster this way
	for(long i = 0; i < pos; i++)
	{
		instruction* inst = cur->inst;

		if(inst->type == OP_SHIFT_R)
		{
			if(inst->val < 0)
			{
				inst->val *= -1;
				inst->type = OP_SHIFT_L;
			}
		}

		memcpy(code + i, inst, sizeof(instruction));

		next = cur->next;
		free(cur);
		free(inst);
		cur = next;
	}
}

void run_program()
{
	for(;;)
	{
		//(*operations[code->type])(code->val);
		
		switch(code->type)
		{
			case OP_BOPEN:
				if(cur->data[chunk_pos] == 0) code += code->val;
				break;
			case OP_BCLOSE:
				if(cur->data[chunk_pos] != 0) code -= code->val;
				break;
			case OP_SHIFT_R:
				chunk_pos += code->val;
				while(chunk_pos >= CHUNK_SIZE)
				{
					if(cur->next == NULL) chunk_init(&cur->next, cur, NULL);
					chunk_pos -= CHUNK_SIZE;
					cur = cur->next;
				}
				break;
			case OP_SHIFT_L:
				chunk_pos -= code->val;
				while(chunk_pos < 0)
				{
					if(cur->prev == NULL) chunk_init(&cur->prev, NULL, cur);
					chunk_pos += CHUNK_SIZE;
					cur = cur->prev;
				}
				break;
			case OP_INC:
				cur->data[chunk_pos] += code->val;
				break;
			case OP_GET:
				cur->data[chunk_pos] = getchar();
				break;
			case OP_PUT:
				putchar(cur->data[chunk_pos]);
				break;
			case OP_STOP:
				return;
		}

		code++;
	}
}

int main(int cargs, const char** vargs)
{
	if(cargs != 2)
	{
		fprintf(stderr, "Usage: %s <filename>\n\n", vargs[0]);
		
		return 1;
	}

	read_file(vargs[1]);

	chunk_init(&cur, NULL, NULL);
	chunk_pos = CHUNK_SIZE / 2;
	
	run_program();

	return 0;
}
