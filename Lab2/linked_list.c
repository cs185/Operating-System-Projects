#include "linked_list.h"
#include <string.h>

struct linked_list *create_linked_list()
{
  struct linked_list *list = (struct linked_list *)malloc(sizeof(struct linked_list));

  struct linked_list_node *head = (struct linked_list_node *)malloc(sizeof(struct linked_list_node));
  memset(head, 0, sizeof(struct linked_list_node));

  struct linked_list_node *tail = (struct linked_list_node *)malloc(sizeof(struct linked_list_node));
  memset(tail, 0, sizeof(struct linked_list_node));

  head->next = tail;
  tail->prev = head;

  list->head = head;
  list->tail = tail;
  return list;
}

struct linked_list_node *create_linked_list_node(void *val)
{
  struct linked_list_node *node = (struct linked_list_node *)malloc(sizeof(struct linked_list_node));
  memset(node, 0, sizeof(struct linked_list_node));
  node->val = val;
  return node;
}

// add a node to the linked list
void add_to_linked_list(struct linked_list *list, void *val)
{
  struct linked_list_node *node = create_linked_list_node(val);
  struct linked_list_node *tail = list->tail;
  struct linked_list_node *prev = tail->prev;

  prev->next = node;
  node->prev = prev;
  node->next = tail;
  tail->prev = node;
}

void remove_from_linked_list(struct linked_list_node *node)
{
  struct linked_list_node *prev = node->prev;
  struct linked_list_node *next = node->next;

  prev->next = next;
  next->prev = prev;
}

void print_linked_list(struct linked_list *list, void (*print)(void *))
{
  struct linked_list_node *node = list->head->next;
  while (node->next != list->tail)
  {
    print(node->val);
    node = node->next;
  }
}

struct linked_list_node *get_next_linked_list_node(struct linked_list_node *node)
{
  if (node->next == NULL)
    return NULL;
  else
  {
    if (node->next->val == NULL)
      // NULL means this is a dummy node
      return NULL;
    else
      return node->next;
  }
}