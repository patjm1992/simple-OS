#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h> 
#include <sys/types.h> 
#include <sys/stat.h>  
#include <fcntl.h>

char *programs[10] = {"programs/fib0.txt", "programs/fib1.txt", "programs/fib2.txt", "programs/fib3.txt", "programs/fib4.txt",
		      "programs/fib5.txt", "programs/fib6.txt", "programs/fib7.txt", "programs/fib8.txt", "programs/fib9.txt"};

/* Process Control Block */
struct PCB {
	int PID;
        char *name;
	int R0, R1, R2, R3;
	short int P0, P1, P2, P3;
	int ACC;
	short int PC;
	int BAR, EAR, LR;
        int timeslice;
	char PSW[2];
	char IR[6];
	struct PCB *next;
};

struct PCB *RQ_h = NULL;    
struct PCB *RQ_t = NULL;    

/* Virtual machine variables */
char IR[6]; 
short int PC = 0; 
short int P0, P1, P2, P3;        
int R0, R1, R2, R3;             
int ACC;
char PSW[2];
char memory [1000][6];    // Now 1000x6

/* MMU components */
int BAR, LR, EAR;

int IC;

/* char --> int */
int ctoi(char c)
{
	return  c - 48;
}

/* int --> char */
char itoc(int n)
{
	return n + 48;
}

/* Given row of memory 'n' (the address), return the value stored there. 
   This had to be modified to adjust for the EAR. */
int get_contents_of(int n)
{
	int i;
	int j = 3;
	int val = 0;

	n = n + BAR;
     
	for(i = 2; i < 6; i++) {
		val += ctoi(memory[n][i]) * (int)pow(10, j);
		j--;
	}

	return val;
}

/* This function stores data in memory. Modified for multiplexing;
   Uses EAR. */
void store_in_memory(short int n, short int location)
{
	location = location + BAR;
		
	int i;

	memory[location][0] = 'Z';  // As this is data
	memory[location][1] = 'Z';
     
	// Splitting the integer so each digit is in a cell of the array 
	for(i = 5; i > 1; i--) {
		memory[location][i] = itoc(n % 10);    // gets a digit (also coverts to char for storing in memory array)
		n = n / 10;
	}
}

/* Select and return Rn register. */
int *select_gen_reg(short int n)
{
	if (n == 0)
		return &R0;
	else if (n == 1)
		return &R1;
	else if (n == 2)
		return &R2;
	else 
		return &R3;
}

/* Select and return Pn register. */
short int *select_ptr_reg(short int n)
{
	if (n == 0)
		return &P0;
	else if (n == 1)
		return &P1;
	else if (n == 2)
		return &P2;
	else 
		return &P3;
}

/* Instructions */
void load_ptr_immed(int val, int n)
{
	*select_ptr_reg(n) = val;
}
     
void addto_ptr_immed(int val, int n)
{
	*select_ptr_reg(n) += val;
}

void subfrom_ptr_immed(int val, int n)
{
	*select_ptr_reg(n) -= val;
} 

void load_acc_immed(int val)
{
	ACC = val;
}

void load_acc_regaddr(int ptr_reg)
{
	get_contents_of(*select_ptr_reg(ptr_reg));
}

void load_acc_diraddr(int addr)
{
	ACC = get_contents_of(addr);
}

void store_acc_regaddr(int ptr_reg)
{
	store_in_memory(ACC, *select_ptr_reg(ptr_reg));
}

void store_acc_diraddr(int addr)
{
	store_in_memory(ACC, addr); 
}

void store_reg_regaddr(int gen_reg, int ptr_reg)
{
	store_in_memory(*select_gen_reg(gen_reg), *select_ptr_reg(ptr_reg));
}

void store_reg_diraddr(int n, int addr)
{
	store_in_memory(*select_gen_reg(n), addr);
}

void load_reg_regaddr(int n, int ptr_reg)
{
	*select_gen_reg(n) = get_contents_of(*select_ptr_reg(ptr_reg));
}

void load_reg_diraddr(int n, int addr)
{
	*select_gen_reg(n) = get_contents_of(addr);
}

void load_R0_immed(int val)
{
	R0 = val;
}

void reg_transfer(int n, int p)
{
	*select_gen_reg(n) = *select_gen_reg(p);
}

void load_acc_from_reg(int n)
{
	ACC = *select_gen_reg(n);
}

void load_reg_from_acc(int n)
{
	*select_gen_reg(n) = ACC;
}

void add_acc_immed(int val)
{
	ACC += val;
}

void sub_acc_immed(int val)
{
	ACC -= val;
}

void add_reg_to_acc(int n)
{
	ACC += *select_gen_reg(n);
}

void sub_reg_from_acc(int n)
{
	ACC -= *select_gen_reg(n);
}

void add_acc_regaddr(int n)
{
	ACC += get_contents_of(*select_ptr_reg(n));
}

void add_acc_diraddr(int addr)
{
	ACC = ACC + get_contents_of(addr);
}

void sub_from_acc_regaddr(int n)
{
	ACC -= get_contents_of(*select_ptr_reg(n));
}

void sub_from_acc_diraddr(int addr)
{
	ACC = ACC - get_contents_of(addr);
}

void comp_eq_regaddr(int n)
{
	if (ACC == get_contents_of(*select_ptr_reg(n)))
		PSW[0] = 'T';
	else
		PSW[0] = 'F';
}

void comp_less_regaddr(int n)
{
	if (ACC < get_contents_of(*select_ptr_reg(n)))
		PSW[0] = 'T';
	else
		PSW[0] = 'F';
}

void comp_greater_regaddr(int n)
{
	if (ACC > get_contents_of(*select_ptr_reg(n)))
		PSW[0] = 'T';
	else
		PSW[0] = 'F';
}

void comp_greater_immed(int val)
{
	if (ACC > val) 
		PSW[0] = 'T';
	else 
		PSW[0] = 'F';
}

void comp_eq_immed(int val)
{
	if (ACC == val)
		PSW[0] = 'T';
	else 
		PSW[0] = 'F';
}

void comp_less_immed(int val)
{
	if (ACC < val)
		PSW[0] = 'T';
	else
		PSW[0] = 'F';
}

void comp_reg_eq(int n)
{
	if (ACC == *select_gen_reg(n))
		PSW[0] = 'T';
	else
		PSW[0] = 'F';
}

void comp_reg_less(int n)
{
	if (ACC < *select_gen_reg(n))
		PSW[0] = 'T';
	else
		PSW[0] = 'F';
}

void comp_reg_greater(int n)
{
	if (ACC > *select_gen_reg(n))
		PSW[0] = 'T';
	else
		PSW[0] = 'F';
}

void branch_cond_t(int addr)
{
	if (PSW[0] == 'T')
		PC = addr;
}

void branch_cond_f(int addr)
{
	if (PSW[0] == 'F')
		PC = addr;
}

void branch_uncond(int addr)
{
	PC = addr;
}

/* Generate random number to serve as a time slice. */
int get_rand()
{
	return (rand() % 10) + 1;    // Hopefully this is random enough
}

/* Display PBVM state; perform memory dump. */
void print_final_state() {
	int i, j;
	printf("\n\nCPU:\n");
	printf("PC = %d SP = n/a ACC = %d PSW = %d IR = ", PC, ACC, PSW[0]);
	for(i = 0; i < 6; i++)
		printf("%c", IR[i]);
     
	printf("\n\nREGISTERS:\n");
	printf("P0 = %d P1 = %d P2 = %d P3 = %d\n", P0, P1, P2, P3);
	printf("R0 = %d R1 = %d R2 = %d R3 = %d\n\n", R0, R1, R2, R3);

	printf("MEMORY:\n");
	for(i = 0; i < 1000; i++) {
		if (i < 10)
			printf("0%d: ", i);
		else 
			printf("%d: ", i);
		for (j = 0; j < 6; j++)
			printf("%c", memory[i][j]);
		printf("\n");
	}
}

/* Given array of size 2, return the integer representation. */
int int_from_array2(int digit1, int digit2)
{	
	return (digit1 * 10) + digit2;
}

/* Given array of size 4, return the integer representation. */
int int_from_array4(int digit1, int digit2, int digit3, int digit4)
{
	return ((digit1 * pow(10, 3)) + (digit2 * pow(10, 2)) + (digit3 * 10) + digit4);
}

/* Display PCBs on the RQ, w/ important fields displayed. */
void print_list_verbose() {
	struct PCB *tmp = RQ_h;
	while (tmp != NULL) {
		printf("\nPID: %d\nNAME: %s\nBAR: %d\nLR: %d\nTS: %d\n"
		       , tmp->PID, tmp->name, tmp->BAR, tmp->LR, tmp->timeslice);
		tmp = tmp->next;
	}
	printf("\n");
}

/* Display RQ, w/ PCB PIDs only. */
void print_list() {
	struct PCB *tmp = RQ_h;
	while (tmp != NULL) {
		printf("%d ", tmp->PID);
		tmp = tmp->next;
	}
	printf("\n\n");
}

/* Given the IR, return the integer representation of the value stored within it.
   This function makes the big switch statement (instruction execution section) 
   less ugly (I think). */
int parse_IR(int op)
{
	if (op == 3 || op == 12 || op == 16 || op == 17 || op == 27 || op == 28 || op == 29) 
		return int_from_array4(ctoi(IR[2]), ctoi(IR[3]), ctoi(IR[4]), ctoi(IR[5]));
	else if (op == 5 || op == 7 || op == 21 || op == 23 || op == 33 || op == 34 || op == 35)
		return int_from_array2(ctoi(IR[2]), ctoi(IR[3]));
	else
		return int_from_array2(ctoi(IR[4]), ctoi(IR[5]));
}

/* PC + BAR gives effective address. */
int get_EAR()
{
	return PC + BAR;
}

/* Return 1 if current address is a legal address for the currently executing
   process. */
int is_legal()
{
	EAR = get_EAR();
	if (EAR <= LR) 
		return 1;
	else 
		return 0;
}

/* Restore values saved in a PCB to the VM variables. As 'move_to_tail()' was called
   immediately after 'save()', it is guaranteed that we are restoring the new head
   of the RQ. */
void restore()
{
	P0 = RQ_h->P0;
	P1 = RQ_h->P1;
	P2 = RQ_h->P2;
	P3 = RQ_h->P3;
	R0 = RQ_h->R0;
	R1 = RQ_h->R1;
	R2 = RQ_h->R2;
	R3 = RQ_h->R3;
	ACC = RQ_h->ACC;

	int i;
	for (i = 0; i < 2; i++) 
		PSW[i] = RQ_h->PSW[i];
	
	for (i = 0; i < 6; i++) 
		IR[6] = RQ_h->IR[6];

	BAR = RQ_h->BAR;
	LR = RQ_h->LR;
	EAR = RQ_h->EAR;
	PC = RQ_h->PC;

	/* Get a new time slice. */
	RQ_h->timeslice = get_rand();
}

/* Save the current state of the VM into the preempted process' PCB. */
void save()
{
	RQ_h->P0 = P0;
	RQ_h->P1 = P1;
	RQ_h->P2 = P2;
	RQ_h->P3 = P3;
	RQ_h->R0 = R0;
	RQ_h->R1 = R1;
	RQ_h->R2 = R2;
	RQ_h->R3 = R3;
	RQ_h->ACC = ACC;

	int i;
	for (i = 0; i < 2; i++) 
		RQ_h->PSW[i] = PSW[i];

	for (i = 0; i < 6; i++) 
		RQ_h->IR[i] = IR[i];

	RQ_h->BAR = BAR;
	RQ_h->LR = LR;
	RQ_h->EAR = EAR;
	RQ_h->PC = PC;
}


/* Move current RQ_h to the end of the queue. */
void move_to_tail() {
	struct PCB *tmp;
	
	if (RQ_h->next == NULL) {
		/* Last PCB remaining in the list. */
		return;
	}

	tmp = RQ_h;
	RQ_h = RQ_h->next;
	RQ_t->next = tmp;
	tmp->next = NULL;
	RQ_t = tmp;    // Important step!
}

/* Remove PCB that has finished execution. */
void remove_PCB()
{
	if (RQ_h->next == NULL) {
		/* This is the last PCB in the RQ. */
		return;
	}
	
	struct PCB *tmp;

	tmp = RQ_h;
	RQ_h = RQ_h->next;
	tmp->next = NULL;
}

/* Print process info before it is given control of the CPU. */
void print_proc_info()
{
	printf("Process %d ready to begin execution.\n", RQ_h->PID);
	printf("It has a time slice of %d instructions.\n", RQ_h->timeslice);
	printf("It is executing program %s.\n\n", RQ_h->name);
}

/* Perform a context switch when a process is preempted. */
void context_switch()
{
	/* Put the contents VM registers/values into the head PCB of the RQ. */
	save();
	/* Move that head PCB to the end of the RQ. New PCB at head now. */
	move_to_tail();
	print_list();
	/* Put the contents of the head PCB of the RQ into the VM registers/variables. */
	restore();
	/* Load the instruction counter register with the head PCB's new time slice. 
	   The time slice was recalculated when the PCB was restored. */
	IC = RQ_h->timeslice;
	/* Print out process info before it is given control of the CPU. */
	print_proc_info();
}

/* Process info to be printed before each instruction execution. */
void print_exec_info()
{
	printf("PID: %d, Instruction: ", RQ_h->PID);
	int i;
	for (i = 0; i < 6; i++) 
		printf("%c", IR[i]);
	printf(", IC: %d\n", IC);
	printf("PC: %d, EAR: %d\n", PC, EAR);
}

int main(int argc, char *argv[]) {

	int i, j;
	int fp;
	int opcode;
	int prog_line;
	char input_line[7];
	
	printf("Reading programs into memory...\n");
	
	/* Read 10 programs into memory. */
	for (i = 0; i < 10; i++) {

		fp = open(programs[i], O_RDONLY);
		printf("Open is %d\n", fp);

		if (fp < 0)
			printf("Could not open file\n");

		int ret = read(fp, input_line, 7);

		while (1) {
			if (ret <= 0) 
				break;
			
			for (j = 0; j < 6; j++) 
				memory[prog_line][j] = input_line[j];
			
			ret = read(fp, input_line, 7);
			prog_line++;
		}
		close(fp);
	}

	/* Set up the PCB structs. */
	struct PCB *curr;

	/* Initialize the head of the RQ (the first program read in). */
	RQ_h = (struct PCB *) malloc (sizeof (struct PCB));
	RQ_h->PID = 0;
	RQ_h->name = programs[0];
	RQ_h->BAR = 0;
	RQ_h->LR = 99;
	RQ_h->EAR = 0;
	RQ_h->timeslice = get_rand();

	/* This process will be first to execute, so load register/values
	   in the VM. Can use the existing restore() function to execute) for this. */
	restore();
	
	/* Initial set of the IC -- straight from the first program read in. */
	IC = RQ_h->timeslice;
			
	curr = RQ_h;

	/* Create and link the rest of the PCBs. */
	for (i = 1; i < 10; i++) {
		curr->next = (struct PCB *) malloc(sizeof(struct PCB));
		curr->next->PID = i;
		curr->next->name = programs[i];
		curr->next->PC = 0;
		curr->next->BAR = i * 100;
		curr->next->LR = curr->next->BAR + 99;
		curr->next->timeslice = get_rand();
		curr = curr->next;
	}
	
	RQ_t = curr;
	RQ_t->next = NULL;

	printf("Programs read into memory.\n");	  

	print_list_verbose();
	print_list();
	printf("\n");

	int ptr_reg, gen_reg, gen_reg_tmp;
	int val;  

	print_proc_info();
	
	/* Go through program(s) in memory and execute instructions. */
	for (i = 0; i < prog_line; i++) {
		printf("Time Slice: %d\n", RQ_h->timeslice);

	        /* Check if it is time for this process to be preempted. */
		if (IC == 0) {
			printf("Process %d completed time slice. Placing at tail of RQ.\n", RQ_h->PID);
			context_switch();
		}

		/* Check to see if process is operating within its legal address space. */
		if (is_legal() == 0) {
			printf("Process %d terminated.\n", RQ_h->PID);
			remove_PCB();
			print_proc_info();
			restore();    // Give next PCB the CPU
			IC = RQ_h->timeslice;
			continue;
		}
		
		opcode = ((int) memory[EAR][0] - 48) * 10;
		opcode += ((int) memory[EAR][1] - 48);

		/* Load up Instruction Register with an instruction to be executed. */
		for(j = 0; j < 6; j++) {
			IR[j] = memory[EAR][j];
		}

		/* Required info output before each instruction. */
		print_exec_info();

	  	/* Execute instruction based on opcode. */
		switch (opcode) {
		case 0:
			printf("Load pointer immediate; ");
			ptr_reg = ctoi(IR[3]);
			val = parse_IR(opcode);
			load_ptr_immed(val, ptr_reg);
			break;
		case 1:
			printf("Add to pointer immediate; ");
			ptr_reg = ctoi(IR[3]);
			val = parse_IR(opcode);
			addto_ptr_immed(val, ptr_reg);
			break;
		case 2:
			printf("Subtract from pointer immediate; ");
			ptr_reg = ctoi(IR[3]);
			val = parse_IR(opcode);
			subfrom_ptr_immed(val, ptr_reg);
			break;
		case 3:
			printf("Load accumulator immediate; ");
			val = parse_IR(opcode);
			load_acc_immed(val);
			break;
		case 4: 
			printf("Load accumulator register addressing; ");
			ptr_reg = ctoi(IR[3]);
			load_acc_regaddr(ptr_reg);
			break;
		case 5: 
			printf("Load accumulator direct addressing; ");
			val = int_from_array2(ctoi(IR[2]), ctoi(IR[3]));
			load_acc_diraddr(val);
			break;
		case 6: 
			printf("Store accumulator register addressing; ");
			ptr_reg = ctoi(IR[3]);
			store_acc_regaddr(ptr_reg);
			break;
		case 7:
			printf("Store accumulator direct addressing; ");
			val = parse_IR(opcode);
			store_acc_diraddr(val);
			break;
		case 8:
			printf("Store register to memory: register addressing; ");
			gen_reg = ctoi(IR[3]);
			ptr_reg = ctoi(IR[5]);
			store_reg_regaddr(gen_reg, ptr_reg);
			break;
		case 9: 
			printf("Store register to memory: direct addressing; ");
			val = parse_IR(opcode);
			store_reg_diraddr(gen_reg, val);
			break;
		case 10: 
			printf("Load register from memory: register addressing; ");
			gen_reg = ctoi(IR[3]);
			ptr_reg = ctoi(IR[5]);
			load_reg_regaddr(gen_reg, ptr_reg);
			break;
		case 11: 
			printf("Load register to memory: direct addressing; ");
			gen_reg = ctoi(IR[3]);
			val = parse_IR(opcode);
			load_reg_diraddr(gen_reg, val);
			break;
		case 12: 
			printf("Load register R0 immediate; ");
			val = parse_IR(opcode);
			load_R0_immed(val);
			break;
		case 13: 
			printf("Register to register transfer; ");
			gen_reg = ctoi(IR[3]);
			gen_reg_tmp = ctoi(IR[5]);
			reg_transfer(gen_reg, gen_reg_tmp);
			break;
		case 14:
			printf("Load accumulator from register; ");
			gen_reg = ctoi(IR[3]);
			load_acc_from_reg(gen_reg);
			break;
		case 15:
			printf("Load register from accumulator; ");
			gen_reg = ctoi(IR[3]);
			load_reg_from_acc(gen_reg);
			break;
		case 16:
			printf("Add accumulator immediate; ");
			val = parse_IR(opcode);
			add_acc_immed(val);
			break;
		case 17:
			printf("Subtract accumulator immediate; ");
			val = parse_IR(opcode);
			sub_acc_immed(val);
			break;
		case 18: 
			printf("Add contents of register to accumulator; ");
			gen_reg = ctoi(IR[3]);
			add_reg_to_acc(gen_reg);
			break;
		case 19: 
			printf("Subtract contents of register to accumulator; ");
			gen_reg = ctoi(IR[3]);
			sub_reg_from_acc(gen_reg);
			break;
		case 20:
			printf("Add accumulator register addressing; ");
			ptr_reg = ctoi(IR[3]);
			add_acc_regaddr(ptr_reg);
			break;
		case 21:
			printf("Add accumulator direct addressing; ");
			val = parse_IR(opcode);
			add_acc_diraddr(val);
			break;
		case 22:
			printf("Subtract from accumulator register addressing; ");
			ptr_reg = ctoi(IR[3]);
			sub_from_acc_regaddr(ptr_reg);
			break;
		case 23:
			printf("Subtract from accumulator: direct addressing; ");
			val = parse_IR(opcode);
			sub_from_acc_diraddr(val);
			break;
		case 24:
			printf("Compare equal register addressing; ");
			ptr_reg = ctoi(IR[3]);
			comp_eq_regaddr(ptr_reg);
			break;
		case 25:
			printf("Compare less register addressing; ");
			ptr_reg = ctoi(IR[3]);
			comp_less_regaddr(ptr_reg);
			break;
		case 26:
			printf("Compare greater register addressing; ");
			ptr_reg = ctoi(IR[3]);
			comp_greater_regaddr(ptr_reg);
			break;
		case 27:
			printf("Compare greater immediate; ");
			val = parse_IR(opcode);
			comp_greater_immed(val);
			break;
		case 28:
			printf("Compare equal immediate; ");
			val = parse_IR(opcode);
			comp_eq_immed(val);
			break;
		case 29:
			printf("Compare less immediate; ");
			val = parse_IR(opcode);
			comp_less_immed(val);
			break;
		case 30: 
			printf("Compare register equal; ");
			gen_reg = ctoi(IR[3]);
			comp_reg_eq(gen_reg);
			break;
		case 31:
			printf("Compare register less; ");
			gen_reg = ctoi(IR[3]);
			comp_reg_less(gen_reg);
			break;
		case 32:
			printf("Compare register greater; ");
			gen_reg = ctoi(IR[3]);
			comp_reg_greater(gen_reg);
			break;
		case 33:
			printf("Branch conditional (if PSW[0] = F); ");
			val = parse_IR(opcode);
			branch_cond_t(val);
			break;
		case 34: 
			printf("Branch conditional (if PSW[0] = F); ");
			val = parse_IR(opcode);
			branch_cond_f(val);
			break;
		case 35:
			printf("Branch unconditional; ");
			val = parse_IR(opcode);
			branch_uncond(val);
			break;
		case 99:
			printf("HALT\n");
			break;	       
		}

		if (opcode != 35 && opcode != 34 && opcode != 33) {
			PC++;
			EAR = get_EAR();
		}
		
		printf("Instruction finished.\n\n");
		IC--;
		
		if (j == 1) 
			break;
	}
	
	print_final_state();

	return 0;
}
