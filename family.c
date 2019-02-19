#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "family.h"

/* Number of word pointers allocated for a new family.
   This is also the number of word pointers added to a family
   using realloc, when the family is full.
*/
static int family_increment = 0;


/* Set family_increment to size, and initialize random number generator.
   The random number generator is used to select a random word from a family.
   This function should be called exactly once, on startup.
*/
void init_family(int size) {
    family_increment = size;
    srand(time(0));
}


/* Given a pointer to the head of a linked list of Family nodes,
   print each family's signature and words.
*/
void print_families(Family *fam_list) {
    int i;
    Family *fam = fam_list;

    while (fam) {
        printf("***Family signature: %s Num words: %d\n",
               fam->signature, fam->num_words);
        for (i = 0; i < fam->num_words; i++) {
            printf("     %s\n", fam->word_ptrs[i]);
        }
        printf("\n");
        fam = fam->next;
    }
}


/* Return a pointer to a new family whose signature is 
   a copy of str. Initialize word_ptrs to point to 
   family_increment+1 pointers, numwords to 0, 
   max_words to family_increment, and next to NULL.
*/
Family *new_family(char *str) {
    Family *new_family_ptr = malloc(sizeof(Family));
    if (new_family_ptr == NULL) {
        perror("malloc error new family pointer in new_family");
        exit(1);
    }
    new_family_ptr->signature = malloc((strlen(str) + 1) * sizeof(char));
    if (new_family_ptr->signature == NULL) {
        perror("malloc error in signature in new_family");
        exit(1);
    }
    strcpy(new_family_ptr->signature, str);
    new_family_ptr->word_ptrs = malloc(sizeof(char *) * (family_increment + 1));
    if (new_family_ptr->word_ptrs == NULL) {
        perror("malloc error in words in new_family");
        exit(1);
    }
    for (int i = 0; i < family_increment + 1; i++) {
        new_family_ptr->word_ptrs[i] = NULL;
    }
    new_family_ptr->num_words = 0;
    new_family_ptr->max_words = family_increment;
    new_family_ptr->next = NULL;
    return new_family_ptr;
}


/* Add word to the next free slot fam->word_ptrs.
   If fam->word_ptrs is full, first use realloc to allocate family_increment
   more pointers and then add the new pointer.
*/
void add_word_to_family(Family *fam, char *word) {
    if ((fam->max_words) > (fam->num_words)) {
        fam->word_ptrs[fam->num_words] = word;
    } else {
        fam->word_ptrs = realloc(fam->word_ptrs, sizeof(char *) * (fam->max_words + family_increment + 1));
        if (fam->word_ptrs == NULL) {
            perror("realloc error in add_word_to_family");
            exit(1);
        }
        fam->word_ptrs[fam->max_words + family_increment] = NULL; // The last one
        fam->word_ptrs[fam->max_words + 1] = NULL; // The next one
        fam->max_words += family_increment;
        fam->word_ptrs[fam->num_words] = word;
    }
    fam->num_words += 1;
}


/* Return a pointer to the family whose signature is sig;
   if there is no such family, return NULL.
   fam_list is a pointer to the head of a list of Family nodes.
*/
Family *find_family(Family *fam_list, char *sig) {
    Family *cur_fam = fam_list;
    while (cur_fam != NULL) {
        if (strcmp(cur_fam->signature, sig) == 0) {
            return cur_fam;
        }
        cur_fam = cur_fam->next;
    }
    return NULL;
}


/* Return a pointer to the family in the list with the most words;
   if the list is empty, return NULL. If multiple families have the most words,
   return a pointer to any of them.
   fam_list is a pointer to the head of a list of Family nodes.
*/
Family *find_biggest_family(Family *fam_list) {
    Family *current_family = fam_list;
    Family *the_chosen_one = current_family;
    while (current_family != NULL) {
        if (current_family->num_words > the_chosen_one->num_words) {
            the_chosen_one = current_family;
        }
        current_family = current_family->next;
    }
    return the_chosen_one;
}


/* Deallocate all memory rooted in the List pointed to by fam_list. */
void deallocate_families(Family *fam_list) {
    while (fam_list != NULL) {
        Family *next = fam_list->next;
        free(fam_list->signature);
        free(fam_list->word_ptrs);
        free(fam_list);
        fam_list = next;
    }
}

/* Return whether or not word contains letter
    - returns 1 if it contains such letter
    - returns 0 if it does not
	
	This is a helper function to speed up whether or not if we need
	to extract the signature, by calling extract_signature.
*/
int contains(char *word, char letter) {
    int i = 0;
    while (word[i] != '\0') {
        if (word[i] == letter) {
            return 1;
        }
        i += 1;
    }
    return 0;
}

/* Extract the signature of word, partitioning using letter. Store
the result through the char array pointer signature. This way I 
don't have to use heap to exchange the signature information.
*/
void extract_signature(char *word, char letter, char *signature) {
    for (int i = 0; i < strlen(word); i++) {
        if (word[i] == letter) {
            signature[i] = letter;
        } else {
            signature[i] = '-';
        }
    }
    signature[strlen(word)] = '\0';
}

/* Generate and return a linked list of all families using words pointed to
   by word_list, using letter to partition the words.
*/
Family *generate_families(char **word_list, char letter) {
    int i = 0;
    /* extarcted_sig is to store the signature of the word that we
    are currentlt looking at, partitione used letter */
    char extracted_sig[128];
    extract_signature(word_list[i], letter, extracted_sig);
    Family *families = new_family(extracted_sig);
    add_word_to_family(families, word_list[i]);
    i += 1;
    while (word_list[i] != NULL) {
        extract_signature(word_list[i], letter, extracted_sig);
        Family *existing = find_family(families, extracted_sig);
        if (existing == NULL) {
            Family *new_fam = new_family(extracted_sig);
            add_word_to_family(new_fam, word_list[i]);
            new_fam->next = families;
            families = new_fam;
        } else {
            add_word_to_family(existing, word_list[i]);
        }
        i += 1;
    }
    return families;
}


/* Return the signature of the family pointed to by fam. */
char *get_family_signature(Family *fam) {
    return fam->signature;
}


/* Return a pointer to word pointers, each of which
   points to a word in fam. These pointers should not be the same
   as those used by fam->word_ptrs (i.e. they should be independently malloc'd),
   because fam->word_ptrs can move during a realloc.
   As with fam->word_ptrs, the final pointer should be NULL.
*/
char **get_new_word_list(Family *fam) {
    char **new_word_list = malloc(sizeof(char *) * (fam->num_words + 1));
    if (new_word_list == NULL) {
        perror("malloc error in get_new_word_list");
        exit(1);
    }
    int i = 0;
    for (; i < fam->num_words; i++) {
        new_word_list[i] = fam->word_ptrs[i];
    }
    new_word_list[fam->num_words] = NULL;
    return new_word_list;
}


/* Return a pointer to a random word from fam.
   Used when the user lose and the computer opponent randomly
   chooses one word that doesn't contain any of the
   inputted characters.
*/
char *get_random_word_from_family(Family *fam) {
    int family_word_count = fam->num_words;
    int chosen_index = rand() % family_word_count;
    return (fam->word_ptrs)[chosen_index];
}
