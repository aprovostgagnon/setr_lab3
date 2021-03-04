#include "commMemoirePartagee.h"

// Appelé au début du programme pour l'initialisation de la zone mémoire (cas du lecteur)
int initMemoirePartageeLecteur(const char* identifiant,
                                struct memPartage *zone){
    return 0;
                                }

// Appelé au début du programme pour l'initialisation de la zone mémoire (cas de l'écrivain)
int initMemoirePartageeEcrivain(const char* identifiant,
                                struct memPartage *zone,
                                size_t taille,
                                struct memPartageHeader* headerInfos){
    return 0;

                                }

// Appelé par le lecteur pour se mettre en attente d'un résultat
int attenteLecteur(struct memPartage *zone){
    return 0;
}

// Fonction spéciale similaire à attenteLecteur, mais asynchrone : cette fonction ne bloque jamais.
// Cela est utile pour le compositeur, qui ne doit pas bloquer l'entièreté des flux si un seul est plus lent.
int attenteLecteurAsync(struct memPartage *zone){
    return 0;
}

// Appelé par l'écrivain pour se mettre en attente de la lecture du résultat précédent par un lecteur
int attenteEcrivain(struct memPartage *zone){
    return 0;
}