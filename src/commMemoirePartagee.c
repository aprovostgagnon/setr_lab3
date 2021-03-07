#include "commMemoirePartagee.h"

// Appelé au début du programme pour l'initialisation de la zone mémoire (cas du lecteur)
int initMemoirePartageeLecteur(const char* identifiant,
                                struct memPartage *zone){
    //Si le lecteur essaye d'ouvrir le fd avant que l'ecrivaint l'est creer, il boucle en attente                              
    while((zone->fd = shm_open(identifiant, O_RDWR, 0666)) < 0);

    //Si l'ecrivaint n'a pas decouper memoire partage (ftruncate) et creer avec mmamp, le lecteur boucle
    struct stat sharedMemStat;    
    do{
        fstat(zone->fd, &sharedMemStat);
    } while(sharedMemStat.st_size == 0);

    //Optient la memoire partage
    void *sharedMem = mmap(NULL, sharedMemStat.st_size, PROT_READ | PROT_WRITE,MAP_SHARED, zone->fd, 0);


    //init la struct qui sert a communique avec la zone partage
    zone->data = (unsigned char*)((uint8_t*)sharedMem + sizeof(memPartageHeader));
    zone->header = (memPartageHeader*)sharedMem;                                               // A voir
    zone->tailleDonnees = sharedMemStat.st_size - sizeof(memPartageHeader);
    zone->copieCompteur = 0;

    // //Attend le que le compte de l'ecrivaint soit de 1
    while(zone->header->frameWriter == 0);
     

    // //Attendre le mutex
    // pthread_mutex_lock(&zone->header->mutex);

    return 0;
}

// Appelé au début du programme pour l'initialisation de la zone mémoire (cas de l'écrivain)
int initMemoirePartageeEcrivain(const char* identifiant,
                                struct memPartage *zone,
                                size_t taille,
                                struct memPartageHeader* headerInfos){

    // creer le file descriptor (fd) qui ser a communiquer entre les processus                                
    if((zone->fd = shm_open(identifiant, O_RDWR | O_CREAT, 0666)) < 0)
        return -1;
    
    // creer une zone de memoire partage de taille taille+header
    ftruncate(zone->fd, taille+sizeof(memPartageHeader));
    void *sharedMem = mmap(NULL, taille, PROT_READ | PROT_WRITE,MAP_SHARED, zone->fd, 0);
    struct memPartageHeader* sharedMemHeader = (memPartageHeader*) sharedMem;
    
    //init le mutex utilise par l'ecrivain et lecteur
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&sharedMemHeader->mutex, &mutex_attr);
    pthread_mutex_lock(&sharedMemHeader->mutex);

    //ecrit le header dans la memoire partage
    
    sharedMemHeader->largeur = headerInfos->largeur;
    sharedMemHeader->hauteur = headerInfos->hauteur;
    sharedMemHeader->frameWriter = 0;
    sharedMemHeader->frameReader = 0;
    sharedMemHeader->fps = headerInfos->fps;
    sharedMemHeader->canaux = headerInfos->canaux;

    //init la struct qui sert a communique avec la zone partage
    zone->data = (unsigned char*)((uint8_t*)sharedMem + sizeof(memPartageHeader));
    zone->header = sharedMemHeader;
    zone->copieCompteur = 0;                                       
    zone->tailleDonnees = taille;

    return 0;
}

// Appelé par le lecteur pour se mettre en attente d'un résultat
int attenteLecteur(struct memPartage *zone){
    while(zone->header->frameWriter == zone->copieCompteur);
    return pthread_mutex_lock(&zone->header->mutex);
}

// Fonction spéciale similaire à attenteLecteur, mais asynchrone : cette fonction ne bloque jamais.
// Cela est utile pour le compositeur, qui ne doit pas bloquer l'entièreté des flux si un seul est plus lent.
int attenteLecteurAsync(struct memPartage *zone){
    if(zone->header->frameWriter == zone->copieCompteur)
        return 1;
    return pthread_mutex_trylock(&zone->header->mutex); //return 0 si le lock est libre
}

// Appelé par l'écrivain pour se mettre en attente de la lecture du résultat précédent par un lecteur
int attenteEcrivain(struct memPartage *zone){
    while(zone->header->frameReader == zone->copieCompteur);
    return pthread_mutex_lock(&zone->header->mutex);
}