#include "gmap.h"

#include <stdio.h>

typedef struct node_instance_t node;

typedef struct _entry
{
  void *key;
  void *value;
} entry;

struct _gmap
{
    node **table; //an array of pointers to heads of chains
    size_t capacity; // how many the buckets are
    size_t size; // how many entries i have total (P1..Pn)

    size_t (*hash)(const void *);
    int (*compare)(const void *, const void *);
    void *(*copy)(const void *);
    void (*free)(void *);
};

//nodes
struct node_instance_t 
{
    entry e;
    node *next;
};

#define GMAP_INITIAL_CAPACITY (29)
#define RESIZE_FACTOR (2)

/**
 * INPUTS:
 *    a gmap m
 * OUTPUTS:
 *    none.
 * RESULT:
 *    embiggens my gmap m by allocating memory to a new bigger array 
 *     and copying over contents
 */
void gmap_embiggen(gmap *m)
{
    if(m->size == m->capacity)
	{
        size_t old_capacity = m->capacity;
		m->capacity = m->capacity * RESIZE_FACTOR;

        //rehash
        node **new_table = calloc(m->capacity, sizeof(node *));

        if(new_table == NULL)
        {
            return;
        }

        for (int i = 0; i < old_capacity; i++) {
            node *current = m->table[i];
            while (current) 
            {
                //shift my nodes and HASH them
                node *next = current->next;
                int index = m->hash(current->e.key) % m->capacity;
                current->next = new_table[index];
                new_table[index] = current;

                current = next;
            }
            //end on NULL
            m->table[i] = NULL;
        }
        //free memory before reassigning
        free(m->table);
        m->table = new_table;
	}
}


void gmap_emsmallen(gmap *m)
{
    //same logic as for embiggen but with different check and capacity reassignment
    if(m->size < (m->capacity / (RESIZE_FACTOR * 2)))
    {
        size_t old_capacity = m->capacity;
        m->capacity = m->capacity / RESIZE_FACTOR;

        //rehash
        node **new_table = calloc(m->capacity, sizeof(node *));
        for (int i = 0; i < old_capacity; i++) {
            node *current = m->table[i];
            while (current) 
            {
                node *next = current->next;
                int index = m->hash(current->e.key) % m->capacity;
                current->next = new_table[index];
                new_table[index] = current;

                current = next;
            }
            m->table[i] = NULL;
        }
        free(m->table);
        m->table = new_table;
    }
}

//return error message
char* gmap_error = "Error\n";


gmap *gmap_create(void *(*cp)(const void *), int (*comp)(const void *, const void *), size_t (*h)(const void *), void (*f)(void *))
{
    if (h == NULL || cp == NULL || comp == NULL || f == NULL)
    {
        // if one of the required functions was missing
        return NULL;
    }

    gmap *result = malloc(sizeof(gmap));
    if (result != NULL)
    {
        // remember the functions used to manipulate the keys
        result->copy = cp;
        result->compare = comp;
        result->hash = h;
        result->free = f;

        // initialize the table
        result->table = calloc(GMAP_INITIAL_CAPACITY, sizeof(node*));
        result->capacity = (result->table != NULL ? GMAP_INITIAL_CAPACITY : 0);
        result->size = 0;
    }
    return result;
}

size_t gmap_size(const gmap *m)
{
    //getter for size
    if (m == NULL)
    {
        return 0;
    }

    return m->size;
}

/**
 * INPUTS:
 *    gmap m, key 
 * OUTPUTS:
 *    outputs index for key according to hash
 * RESULT:
 *    this helper function rehashes my key and modulo's by capacity
 */
size_t hash_helper(const gmap *m, const void *key)
{
    return m->hash(key) % m->capacity;
}

void *gmap_put(gmap *m, const void *key, void *value)
{
    if (m == NULL || key == NULL || value == NULL)
    {
        return gmap_error;
    }

    //embiggen if necessary (will check in the function)
    gmap_embiggen(m);

    //find index
    int index = hash_helper(m, key);

    //assign current node to the index
    node *current = m->table[index];

    node *old_value = NULL;

    while (current) 
    {
        // check if the key already exists in the map
        if (m->compare(current->e.key, key) == 0) {

            //store the old value
            old_value = current->e.value;
        
            current->e.value = value;
            // return updated value
            return old_value;
        }
        // else move on
        current = current->next;
    }

    // if current is null, we have made it to the end of the list without seeing a match
    // so we need to make a new node
    node *new_node = (node*)malloc(sizeof(node));

    if (new_node == NULL)
    {
        return gmap_error;
    }

    // deep copy KEY!
    new_node->e.key = m->copy(key);
    // but just assign the value
    new_node->e.value = value;
    new_node->next = m->table[index];

    m->table[index] = new_node;

    //increase size after we've inserted
    m->size++;

    //return NULL 4 successful insert
    return NULL;
}


void *gmap_remove(gmap *m, const void *key)
{
    //similar logic to what i used for gmap_put
    //the difference is now I shift differently and decrease size
    if (m == NULL || key == NULL) {
        return NULL;
    }

    int index = hash_helper(m, key);
    node *current = m->table[index];
    node *prev = NULL;

    while (current != NULL) 
    {
        if (m->compare(current->e.key, key) == 0) {
            if (prev) 
            {
                prev->next = current->next;
            } 
            else 
            {
                m->table[index] = current->next;
            }
            void *removed_value = current->e.value;
            free(current->e.key);
            free(current);
            m->size--;

            gmap_emsmallen(m);

            return removed_value;
        }
        prev = current;
        current = current->next;
    }
    return NULL;
}

bool gmap_contains_key(const gmap *m, const void *key)
{
    if (m == NULL || key == NULL) {
        //null cases
        return false;
    }

    //find index and assign current node
    int index = hash_helper(m, key);
    node *current = m->table[index];

    while (current) 
    {
        if (m->compare(current->e.key, key) == 0) 
        {
            //using the strcmp function to check if given key exists in list
            return true;
        }
        //keep moving
        current = current->next;
    }
    return false;
}


void *gmap_get(gmap *m, const void *key)
{
    //edge checking
    if (m == NULL || key == NULL) {
        return NULL;
    }

    if(!gmap_contains_key(m, key))
    {
        return NULL;
    }

    int index = hash_helper(m, key);
    node *current = m->table[index];

    while (current) 
    {
        //get the matching node and return it's value
        if (m->compare(current->e.key, key) == 0) 
        {
            return current->e.value;
        }
        current = current->next;
    }
    return NULL;
}

void gmap_for_each(gmap *m, void (*f)(const void *, void *, void *), void *arg)
{
    if (m == NULL || f == NULL)
    {
        return;
    }

    for (int i = 0; i < m->capacity; i++)
    {
        //do this for every node in the capacity
        node *current = m->table[i];
        while (current) 
        {
            //apply the function and keep moving
            f(current->e.key, current->e.value, arg);
            current = current->next;
        }
    }
}

/**
 * Returns an array containing pointers to all of the keys in the
 * given map.  The return value is NULL if there was an error
 * allocating the array.  The map retains ownership of the keys, and
 * the pointers to them are only valid as long until they are removed
 * from the map, or until the map is destroyed, whichever comes first.
 * It is the caller's responsibility to destroy the returned array if
 * it is non-NULL. Behavior is undefined is m is NULL.
 *
 * @param m a pointer to a map, non-NULL
 * @return a pointer to an array of pointers to the keys, or NULL
 */
const void **gmap_keys(gmap *m)
{
    if (m == NULL)
    {
        return NULL;
    }

    const void **keys = NULL;
    int number_of_keys = 0;

    for (int i = 0; i < m->capacity; i++)
    {
        //count number of keys in the array
        node *current = m->table[i];
        while (current) 
        {
            current = current->next;
            number_of_keys++;
        }
    }
    
    //malloc for that amount
    keys = malloc((number_of_keys) * sizeof(const void *));

    if (keys == NULL)
    {
        return NULL;
    }

    int index = 0;

    for (int i = 0; i < m->capacity; i++)
    {
        //iterate through the linked list and put the keys in the array
        node *current = m->table[i];
        while (current)
        {
            keys[index] = current->e.key;
            index++;
            current = current->next;
        }
    }

    return keys;
}

void gmap_destroy(gmap *m)
{
    if (m == NULL)
    {
        return;
    }

    for (int i = 0; i < m->capacity; i++)
    {
        //iterate through all the nodes in each bucket
        //copy it to a temp, free the temp
        node *current = m->table[i];
        while (current) 
        {
            node *temp = current;
            current = current->next;
            m->free(temp->e.key);
            m->free(temp);
        }
    }

    //finally, free the table and map
    free(m->table);
    free(m);
}
