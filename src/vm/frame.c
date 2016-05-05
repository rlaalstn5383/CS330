#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include <hash.h>
#include <list.h>

static bool install_page (void *, void *, bool);
static unsigned page_hash (const struct hash_elem *, void *);
static bool page_less (const struct hash_elem *, const struct hash_elem *, void *);

static struct list frame_table;
static struct hash kpage_hash;

struct frame_entry
{
  void *kpage;
  struct list pte_list;
  struct list_elem elem;
};

struct pte_entry
{
  void *pte;
  struct list_elem elem;
};

struct kpage_f_e
{
  uint8_t *kpage;
  struct frame_entry *frame_entry;
  struct hash_elem elem;
};

void frame_init (void)
{
  list_init (&frame_table);
  hash_init (&kpage_hash, page_hash, page_less, NULL);
}

uint8_t *frame_get_page (enum palloc_flags flags, uint8_t *upage, bool writable)
{
  uint32_t *pte;
  uint8_t *kpage;
  struct frame_entry *f_e;
  struct pte_entry *p_e;
  struct kpage_f_e k_f_e, *temp_k_f_e;
  struct hash_elem *h_e;
  kpage = palloc_get_page (flags);
  if (kpage == NULL)
    return NULL;
  if (!install_page (upage, kpage, writable)) 
  {
    palloc_free_page (kpage);
    return NULL;
  }
  pte = lookup_page (thread_current ()->pagedir, upage, true);
  k_f_e.kpage = kpage;
  h_e = hash_find (&kpage_hash, &k_f_e.elem);
  if (h_e == NULL)
  {
    f_e = (struct frame_entry *) malloc (sizeof (struct frame_entry));
    list_init (&f_e->pte_list);
    temp_k_f_e = (struct kpage_f_e *) malloc (sizeof (struct kpage_f_e));
    temp_k_f_e->kpage = kpage;
    temp_k_f_e->frame_entry = f_e;
    hash_insert (&kpage_hash, &temp_k_f_e->elem);
    list_push_back (&frame_table, &f_e->elem);
  }
  else
  {
    f_e = hash_entry (h_e, struct kpage_f_e, elem)->frame_entry;
  }
  p_e = (struct pte_entry *) malloc (sizeof (struct pte_entry));
  p_e->pte = pte;
  list_push_back (&f_e->pte_list, &p_e->elem);
  return kpage;
}

void frame_free_page (uint32_t *pte)
{
  struct kpage_f_e k_f_e, *temp_k_f_e;
  struct frame_entry *f_e;
  struct pte_entry *p_e;
  struct hash_elem *h_e;
  struct list_elem *e;
  uint8_t *page;

  page = pte_get_page(*pte);

  k_f_e.kpage = page;
  h_e = hash_find (&kpage_hash, &k_f_e.elem);
  if (h_e != NULL)
  {
    temp_k_f_e = hash_entry (h_e, struct kpage_f_e, elem);
    f_e = temp_k_f_e->frame_entry;
    for (e = list_begin (&f_e->pte_list); e != list_end (&f_e->pte_list); e = list_next (e))
    {
      p_e = list_entry (e, struct pte_entry, elem);
      if (p_e->pte == pte)
      {
        list_remove (&p_e->elem);
        free (p_e);
        break;
      }
    }
    if (list_empty (&f_e->pte_list))
    {
      hash_delete (&kpage_hash, h_e);
      free (f_e);
      free (temp_k_f_e);
      palloc_free_page (page);
    }
  }
}
      


    


    
    


static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

static unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct kpage_f_e *p = hash_entry (p_, struct kpage_f_e, elem);
  return hash_bytes (&p->kpage, sizeof p->kpage);
}

static bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct kpage_f_e *a = hash_entry (a_, struct kpage_f_e, elem);
  const struct kpage_f_e *b = hash_entry (b_, struct kpage_f_e, elem);

  return a->kpage < b->kpage;
}

