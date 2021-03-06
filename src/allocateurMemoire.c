#include "allocateurMemoire.h"

#define NB_OF_BLOCK 10

static size_t blockSize = 0;
static void *memBlocks[NB_OF_BLOCK]; 
static unsigned int blockState[NB_OF_BLOCK];  // 0 if not used and 1 if used

// Prépare les buffers nécessaires pour une allocation correspondante aux tailles
// d'images passées en paramètre. Retourne 0 en cas de succès, et -1 si un
// problème (par exemple manque de mémoire) est survenu.
int prepareMemoire(size_t tailleImageEntree, size_t tailleImageSortie){

    blockSize = tailleImageEntree >  tailleImageSortie? tailleImageEntree: tailleImageSortie;
    
    for(int i = 0; i < NB_OF_BLOCK; i++){
        memBlocks[i] = malloc(blockSize);
        blockState[i] = 0;
    }
    //setrlimit();
    //mlockall(); // pas sur de devoir faire ca
    return 0;
}
// Ces deux fonctions doivent pouvoir s'utiliser exactement comme malloc() et free()
// (dans la limite de la mémoire disponible, bien sûr)
void* tempsreel_malloc(size_t taille){
    if(blockSize < taille){
        fprintf(stderr, "\n\ntempsreel_malloc: blocksize is too big \n maxblock: %i\n block needed: %i\n\n", blockSize,taille);
        return NULL;
    }
    for(int i = 0; i < NB_OF_BLOCK; i++){
        if(blockState[i]==0){
            blockState[i] = 1;
            return memBlocks[i];
        }
    }

    fprintf(stderr, "Out of memory \n");
    return NULL;
}

void tempsreel_free(void* ptr){
    for(int i = 0; i < NB_OF_BLOCK; i++){
        if(memBlocks[i] == ptr){
            blockState[i] = 0;
            return;
        }            
    }
    fprintf(stderr, "Memory block not found in memory pool. Cannot free memory\n");
}