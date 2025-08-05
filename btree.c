#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<stdint.h>
#include<string.h>
#include <limits.h>
#define T 3 // minium grade of btree
#define BTREE_MAGIC "MYBTREE\0" //header identifier

struct BTreeNode;
struct BTree;
struct BTreeHeader;

/* Config and struts */
//node of btree
typedef struct BTreeNode {
  int n; // number of keys of the node
  int keys[2 * T - 1]; //arrays of the keys (max. dim)
  bool leaf; //indicates if this node is a leaf
  int64_t c[2 * T]; //offset of the children in file (if we want to use mem-orientend we must have a **c of chidren)
  int64_t self_offset; //position of the node in file
} BTreeNode;


typedef struct BTree{ 
  int t; // minium grade
  FILE *fp; //ptr of the file
  int64_t root_offset; // position of offset of the root in file
  int64_t next_free_offset; //max position free in file for new node
} BTree;

/** 
 * it is used to mantain the state
 * Example: Our program is running in RAM, and we have a BTRee struct with: 
 *  - grade t
 *  - root_offset 
 *  - next_free_offset
 *  Suppose now we close the program, the ram will be cleared and we lost the btree
 *  We need store these info, and BTreeHeader solves this problem, this is a little section in file allowing use to reload tree's metadata
 * */

typedef struct BTreeHeader {
  char magic[8]; //file identifier, this is the first read, if our program open a file, it prevent data corruption and crash
  int t; //grade of the tree
  int64_t root_offset; //offset of the root
  int64_t next_free_offset; //next free position 

} BTreeHeader;

void disk_write(BTree* tree, BTreeNode *node) {
  if(fseek(tree->fp, node->self_offset, SEEK_SET) != 0) { /* start at offset of the node in file*/
    perror("Error seeking to write mode");
    exit(1);
  }

 if(fwrite(node, sizeof(BTreeNode), 1 , tree->fp) != 1){
   perror("Error writing node to disk");
   exit(1);
 }
  // when we call fwrite() the data isn't write immediatly on disk, for efficienmt, the os write it on buffer
  // force the write of the data from buffer to disk, is essential for evict to read old data, before the write is complete
  fflush(tree->fp);
}

BTreeNode* disk_read(BTree* tree, int64_t offset){
  BTreeNode* node = malloc(sizeof(BTreeNode));
  //error allocating a node
  if(!node) {
    perror("Error allocating mem for node");
    exit(1);
  }
  
  if(fseek(tree->fp, offset, SEEK_SET) != 0){
    perror("Error seeking to read node"); 
    free(node);
    exit(1);
  }
  
  if(fread(node, sizeof(BTreeNode), 1, tree->fp) != 1) {
    free(node);
    return NULL;
  }

  return node;
}

//search for a key 'k' from the node at node_offset
BTreeNode* b_tree_search(BTree* tree, int64_t node_offset, int k, int* found_index /* this is used  for return more results in function, 
                                                                                 case found_index is used for return the key-index 
                                                                                 in array of keys */ ) {
  //read node from disk
  BTreeNode* x = disk_read(tree, node_offset);
  int i = 0;
  //find correct position of k in the node
  while(i < x->n && k > x->keys[i]) {
    i++;
  }

  //check if the key is present
  if(i < x->n && k == x->keys[i]) {
   if(found_index){
     *found_index = i;
   }
    return x; // found node!, remember to use free after used this node
  }
  //if is a leaf, the key isn't present
  if(x->leaf) {
    free(x);
    return NULL;
  }
  // we need store variable because we free x after
  int64_t child_offset = x->c[i];
  //if the cases have not been reached , search in the next_node
  free(x);
  return b_tree_search(tree, child_offset, k, found_index);
  
}

BTree* b_tree_create(const char* filename){
  BTree* tree = malloc(sizeof(BTree));
  if(!tree) {
     perror("Failed to allocate mem for btree");
     exit(1);
  }
  //try to open a file in read/write, so we can to open a file and write on it
  tree->fp = fopen(filename, "r+b");
  if(tree->fp == NULL){
     // CASE 1: FILE DOESN'T EXISTS

    //the file doesn't exists, so we create it
    tree->fp = fopen(filename, "w+b");
    if(tree->fp == NULL){
      perror("Impossible to create a file for the BTree");
      free(tree);
      return NULL;
    }
  printf("File '%s' not found. Creating new file of BTree\n", filename);
  
  //STEP A - Creation of header
  BTreeHeader header;
  strcpy(header.magic, BTREE_MAGIC);
  header.t = T;
  //the first node (root) is after header
  header.root_offset = sizeof(BTreeHeader);
  //find the free space after header and the first node
  header.next_free_offset = sizeof(BTreeHeader) + sizeof(BTreeNode);
  
  //Step B - Write header from init of file
  fseek(tree->fp, 0, SEEK_SET);
  if(fwrite(&header, sizeof(BTreeHeader),1,tree->fp) != 1){
    perror("Error during writing header");
    fclose(tree->fp);
    free(tree);
    exit(1);
   }
   

  // Step C create root node in mem
  BTreeNode* root = malloc(sizeof(BTreeNode));
  root->n = 0;
  root->leaf = true;
  root->self_offset = header.root_offset;

  //Step D - Write node on disk
  fseek(tree->fp, root->self_offset, SEEK_SET);
  if (fwrite(root , sizeof(BTreeNode), 1, tree->fp) != 1){
    perror("Error writing root node");
    fclose(tree->fp);
    free(root);
    free(tree);
    exit(1);
  }
  free(root); //free root in the mem
  
  //STEP E - Initialize the strcture in mem after data creation
  tree->t = header.t;
  tree->root_offset = header.root_offset;
  tree->next_free_offset = header.next_free_offset;
} else {
  // CASE TWO:
  
  printf("File %s found. Opening existing B-Tree ", filename);
  BTreeHeader header;

  //Step A Reader header from file
  if(fread(&header, sizeof(BTreeHeader), 1, tree->fp) != 1){
    perror("Error during reading file of btree(maybe empty or corrupted)");
    fclose(tree->fp);
    free(tree);
    exit(1);
  }
  //Step B Verify the magic word
  if(strncmp(header.magic, BTREE_MAGIC, sizeof(header.magic)) != 0) {
    fprintf(stderr,"Error, the file isn't a btree");
    fclose(tree->fp);
    free(tree);
    exit(1);
  }

  //Step C - Initialize the strcucture with data read from file
  tree->t = header.t;
  tree->root_offset = header.root_offset;
  tree->next_free_offset = header.next_free_offset;
 }
 return tree;
}

void b_tree_split_child(BTree* tree, BTreeNode* x, int i) {
  //Step 1 a new node z is created, will be the right-brother of y, we need read from disk that y is full
  BTreeNode* z = malloc(sizeof(BTreeNode));
  z->self_offset = tree->next_free_offset;
  tree->next_free_offset += sizeof(BTreeNode);

  //we need read y from disk using offset
  BTreeNode* y = disk_read(tree, x->c[i]);

  //set properties of z
  z->leaf = y->leaf;
  z->n = T-1; //z will contain T-1 keys greather than y

  /*
   * Step 2 cut operation  
   * we copy the second half of the keys from y to z (the index keys of y goes from 0 to 2T-2)
   * T is the minium grade, its the parameter that define the structure and the capacity of each node
   * T - 1 is the median key ,so indicates the first half of y so j + T is the second half of y because
   * j = 0 so  j + T means the first element after the first half, j = 1 the second element etc etc..
   */
  for(int j = 0; j < T - 1; j++){
    z->keys[j] = y->keys[j + T];
  }
  //if y isn't a leaf, copy the T pointer at the childrens
  if(!y->leaf) {
    for(int j = 0; j < T; j++){
      z->c[j] = y->c[j + T];
    }
  }
  //updates number of keys in y this is only the first half
  y->n = T - 1;

  /* Step 3 , paste operation
   * we need space in the father node x for the children z e for the median key
   * move the pointer to the children in x for space to y
   * Suppose x haves n=4 keys and i = 1. The ptrs to children are [c_0 | c_1 | c_2 | c_3 | c_4]
   * We want to inset z in pos i + 1 = 2, we need move c_2 .. c_4
   * for j = 4 : x->c[5] = x->c[4] the pointer c_4 will be move to right
   * [c_0 | c_1 | c_2 | c_3 | c_4 | c_4]
   * for j = 3 : x->c[4] = x->c[3] the pointer c_3 will move to right
   *  [c_0 | c_1 | c_2 | c_3 | c_3 | c_4 ] 
   *  for j = 2(i+1) : x->c[3] = x->c[2] the pointer c_2 will move to right
   * [c_0 | c_1 | c_2 | c_2 | c_3 | c_4]
   *  The cycle terminates so i=2 can be overwritted
   */

  for(int j = x->n; j>= i + 1; j--){
    x->c[j + 1] = x->c[j];
  }
  
   //insert z as a children of x
   x->c[i+1] = z->self_offset;
   
   //move the keys in x for make room to the median key y
   for(int j = x->n - 1; j >= i; j--) {
     x->keys[j + 1] = x->keys[j];
   }

   /* Step 4 promotion y in the father node 'x'
    * index of median key is 'T - 1' because is the median value 
    * the max keys is 2T - 1 , if we divide /2 we obtain median value 'T - 1'
    */ 
   x->keys[i] = y->keys[T - 1];
   
   //update key size of x 
   x->n = x->n + 1;

   //write on disk
   disk_write(tree, y);
   disk_write(tree, z);
   disk_write(tree, x);

   // FREE MEMORY
   free(y);
   free(z);
}

void b_tree_insert_nonfull(BTree* tree, int64_t root_offset, int k){
  //read node x from the disk
  BTreeNode* x = disk_read(tree, root_offset);

  if(x->leaf){
    /* STEP 1 the node is a leaf
     * the node is a leaf, we need find a pos where insert a new key
     */ 
    int i = x->n - 1;
    while(i >= 0 && k < x->keys[i]){
      //move to the right the key greather to make Iroom
      x->keys[i + 1] = x->keys[i];
      i--;
    }
    //insert a new key
    x->keys[i+1] = k;
    x->n = x->n + 1;
    
    //write the node on disk
     disk_write(tree, x);
  } else {
    // STEP 2 the node isn't a leaf
    // we need find the index of child where we need go down
    int i = x->n - 1;
    while(i >= 0 && k < x->keys[i]) {
      i--;
    }
    i++; //the index correct is after the keys
    //we nee check if the child is full
    BTreeNode* child = disk_read(tree, x->c[i]);
    if(child->n == 2 * T - 1){
      b_tree_split_child(tree, x, i);
      // after split the median key is went up in 'x'
      // we need check if the key 'k' is greather than median key
      if (k> x->keys[i]) i++;
    }
   free(child); // free mem for the check of child is full
   b_tree_insert_nonfull(tree, x->c[i], k); // if the child isn't full call recursively, because is a top-down strategy, for insert a key where the node is same at any level of btree
  }
  free(x);
}

void b_tree_insert(BTree* tree, int k) {
  //read the off-set of current node
  int64_t root_offset = tree->root_offset;
  //load from disk in mem the node
  BTreeNode* root_node = disk_read(tree, root_offset);

  //if the root is full, we need to split it, the tree grow up
  if(root_node ->n == 2 * T -1) {
    // STEP 1 - create a new node 's' that became the new root
    BTreeNode* s = malloc(sizeof(BTreeNode));
    s->self_offset = tree->next_free_offset;
    tree->next_free_offset += sizeof(BTreeNode);
    s->leaf = false;
    s->n = 0;
    /* Step 2 - degrades the old root to children to children of new root
     * the first children c[0] the new root of s became the old root (), the operation split moves the median key to the node father , but the root for have not a father, so where median key can go?
     * If the root need a father, we need create one for the root, the btree grow from up not down, add a level on top, the node 's' is the new level.
     * This operation means to degrade the old root, the first ptr to child c[0] of the new root must point to address of disk root_offset of old root
     * the old root isn't the root of entire btree and became a simple children of new node 's', so we can call b_tree_split_child, because now we have father not full and a full children ready for to be split
     */ 
    s->c[0] = root_offset;
    // Step 4 : split old root
    b_tree_split_child(tree, s, 0);
    // update offset in structure btree
    tree->root_offset = s->self_offset;

    // Step 5: Insert che key 'k' to the new root, after split 's' isn't fulll, so we can start process of insert start by new root 's'
    b_tree_insert_nonfull(tree, s->self_offset, k);

   //Step 6 write root in disk, the node 's' is edited by split, so we need to save it
   disk_write(tree, s);

   free(s);
  } else {
    //the root isn't full so we can insert directly
    b_tree_insert_nonfull(tree, root_offset, k);
  }
  free(root_node);
}
// close tree for save header of tree
void b_tree_close(BTree* tree) {
    if (!tree) return;
    if (tree->fp) {
        printf("Saving header...\n");
        BTreeHeader header;
        strcpy(header.magic, BTREE_MAGIC);
        header.t = tree->t;
        header.root_offset = tree->root_offset;
        header.next_free_offset = tree->next_free_offset;

        fseek(tree->fp, 0, SEEK_SET);
        fwrite(&header, sizeof(BTreeHeader), 1, tree->fp);
        
        fclose(tree->fp);
    }
    free(tree);
}


// Recursive function for print the btree
void print_b_tree_recursive(BTree* tree, int64_t node_offset, int level) {
    if (node_offset == 0) return;

    BTreeNode* node = disk_read(tree, node_offset);
    if (!node) {
        printf("Error reading node at offset %lld\n", (long long)node_offset);
        return;
    }

    for (int i = 0; i < level; i++) {
        printf("  ");
    }

        printf("Node at offset %lld (n=%d, leaf=%s): [", (long long)node->self_offset, node->n, node->leaf ? "yes" : "no");
    for (int i = 0; i < node->n; i++) {
        printf("%d%s", node->keys[i], (i == node->n - 1) ? "" : ", ");
    }
    printf("]\n");

    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++) {
            print_b_tree_recursive(tree, node->c[i], level + 1);
        }
    }

    free(node);
}

// print from root
void print_b_tree(BTree* tree) {
    printf("\n--- B-Tree Structure ---\n");
    if (!tree || tree->root_offset == 0) {
        printf("Tree is empty or not initialized.\n");
        return;
    }
    print_b_tree_recursive(tree, tree->root_offset, 0);
    printf("--- End of B-Tree Structure ---\n\n");
}

void search_and_print(BTree* tree, int key) {
    printf("Searching for key %d... ", key);
    int found_index;
    BTreeNode* found_node = b_tree_search(tree, tree->root_offset, key, &found_index);
    if (found_node) {
        printf("-> Found at offset %lld, index %d.\n", (long long)found_node->self_offset, found_index);
        free(found_node);
    } else {
        printf("-> Not found.\n");
    }
}

int main(void) {
    const char* filename = "extreme_btree.db";
    BTree* tree;

    printf("========= STARTING B-TREE TESTS =========\n\n");

    // --- TEST CASE 1: Valori strani, duplicati, negativi e limiti ---
    printf("--- TEST CASE 1: Duplicate, Negative, and Boundary Values ---\n");
    remove(filename);
    tree = b_tree_create(filename);
    
    printf("Inserting special values...\n");
    b_tree_insert(tree, 100);
    b_tree_insert(tree, -50);
    b_tree_insert(tree, 0);
    b_tree_insert(tree, 100); // duplicate
    b_tree_insert(tree, 200);
    b_tree_insert(tree, -50); // duplicate
    b_tree_insert(tree, INT_MAX);
    b_tree_insert(tree, INT_MIN);

    print_b_tree(tree);
    search_and_print(tree, 100);
    search_and_print(tree, -50);
    search_and_print(tree, 0);
    search_and_print(tree, 999); // doesn't exists
    search_and_print(tree, INT_MAX);
    search_and_print(tree, INT_MIN);
    b_tree_close(tree);
    printf("--- TEST CASE 1 COMPLETE ---\n\n");


    // --- TEST CASE 2: Stress Test con inserimento sequenziale ---
    printf("--- TEST CASE 2: Bulk Sequential Insertion (Stress Test) ---\n");
    remove(filename);
    tree = b_tree_create(filename);
    int sequential_count = 100;
    printf("Inserting %d sequential keys (1 to %d)...\n", sequential_count, sequential_count);
    for (int i = 1; i <= sequential_count; i++) {
        b_tree_insert(tree, i);
    }
    printf("Insertion complete.\n");
    print_b_tree(tree);
    search_and_print(tree, 1);
    search_and_print(tree, sequential_count / 2);
    search_and_print(tree, sequential_count);
    search_and_print(tree, sequential_count + 1); // doesn't exists
    b_tree_close(tree);
    printf("--- TEST CASE 2 COMPLETE ---\n\n");


    printf("--- TEST CASE 3: Bulk Reverse Sequential Insertion (Stress Test) ---\n");
    remove(filename);
    tree = b_tree_create(filename);
    int reverse_count = 100;
    printf("Inserting %d reverse sequential keys (%d to 1)...\n", reverse_count, reverse_count);
    for (int i = reverse_count; i >= 1; i--) {
        b_tree_insert(tree, i);
    }
    printf("Insertion complete.\n");
    print_b_tree(tree);
    search_and_print(tree, 1);
    search_and_print(tree, reverse_count / 2);
    search_and_print(tree, reverse_count);
    b_tree_close(tree);
    printf("--- TEST CASE 3 COMPLETE ---\n\n");


    printf("--- TEST CASE 4: Empty and Single-Node Tree Operations ---\n");
    remove(filename);
    tree = b_tree_create(filename);
    printf("Tree created. It should be empty.\n");
    print_b_tree(tree);
    search_and_print(tree, 42); // search in a empty btree

    printf("\nInserting a single key: 1337\n");
    b_tree_insert(tree, 1337);
    print_b_tree(tree);
    b_tree_close(tree);

    printf("\nReopening single-node tree...\n");
    tree = b_tree_create(filename);
    print_b_tree(tree);
    search_and_print(tree, 1337);
    search_and_print(tree, 1); // doesn't exists
    b_tree_close(tree);
    printf("--- TEST CASE 4 COMPLETE ---\n\n");

    printf("========= ALL TESTS COMPLETED =========\n");
    return 0;
}
