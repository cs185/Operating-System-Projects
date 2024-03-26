#ifndef YALNIX_LINKED_LIST_H
#define YALNIX_LINKED_LIST_H
// this provides a genric linked list implementation

typedef struct linked_list_node
{
  // we will use double linked list
  struct linked_list_node *next;
  struct linked_list_node *prev;

  // the meat
  void *val;
} linked_list_node;

typedef struct linked_list
{
  // the head of the list
  struct linked_list_node *head;
  // the tail of the list
  struct linked_list_node *tail;
  // the size of the list
  int size;
} linked_list;

// create a linked list
struct linked_list *create_linked_list();

// create a linked list node
struct linked_list_node *create_linked_list_node(void *val);

// add a node to the linked list
void add_to_linked_list(struct linked_list *list, void *val);

// remove a node from the linked list, but not free the node
void remove_from_linked_list(struct linked_list_node *node);

// print the linked list with the print function
void print_linked_list(struct linked_list *list, void (*print)(void *));

// get the next linked list node, if it is the last node, return NULL
struct linked_list_node *get_next_linked_list_node(struct linked_list_node *node);

#endif // YALNIX_LINKED_LIST_H
