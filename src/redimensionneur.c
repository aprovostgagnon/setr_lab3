// Gestion des ressources et permissions
#include <sys/resource.h>

// Nécessaire pour pouvoir utiliser sched_setattr et le mode DEADLINE
#include <sched.h>
#include "schedsupp.h"

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"

#define DEFAULT_ORDO "NORT"
#define DEFAULT_DEADLINE_OPTION "1000,1000,1000"
#define DEFAULT_IMAGE_IN "/redimesionIn"
#define DEFAULT_IMAGE_OUT "/redimesionOut"

int main(int argc, char* argv[]){
    

        int debug = 0;
        char imageIn[100] = DEFAULT_IMAGE_IN;
        char imageOut [100] = DEFAULT_IMAGE_OUT;
        char type_ord[100] = DEFAULT_ORDO;
        char options[100] = DEFAULT_DEADLINE_OPTION ;
        int largeur = 1;
        int hauteur = 1;
        int methode = 0;
        struct sched_attr ord;
        int c;

        while ((c = getopt(argc, argv, "awhs:d::m:")) != -1) {
        switch (c) {

        case 'a': 
            debug = 1; 
            break;

        case 'd': 
            strcpy(options, optarg);
            break;

        case 'w': 
            largeur = atoi(optarg); 
            break;
        
         case 'h': 
            hauteur = atoi(optarg); 
            break;

        case 's':
            strcpy(type_ord, optarg);
            break;
        
        case 'm':
            methode = atoi(optarg);
            break;

        default:
            printf("NOOB\n");
            fprintf(stderr, "Option invalide\n");
            exit(EXIT_FAILURE);
        }
    
    }

    // Traite les différentes options
    if (debug == 0) {
        if (argc - optind  != 2) {
            strcpy(imageIn,  argv[optind]);
            strcpy(imageOut,argv[optind + 1]);
        } else {

            fprintf(stderr, "Le decodeur a besoin du  fichier video d'entree et le flux de sortie \n");
            exit(EXIT_FAILURE);
        }

        // Verifie type ordonnancement
        sched_getattr(0, &ord,sizeof(struct sched_attr),0);
        if(strcmp(type_ord, "NORT\0")){
            // default setting
        } else if(strcmp(type_ord, "RR\0")) {
            ord.sched_policy = SCHED_RR;
        } else if (strcmp(type_ord, "FIFO\0")) {
            ord.sched_policy = SCHED_FIFO;
        } else if (strcmp(type_ord, "DEADLINE\0")){
            ord.sched_policy = SCHED_DEADLINE;
            ord.sched_priority = -101;
            ord.sched_runtime = (__u64)atoi(strtok(options,",")); 
            ord.sched_deadline = (__u64)atoi(strtok(options,","));
            ord.sched_period = (__u64)atoi(strtok(options,","));

        } else {
            printf("NO BUENO ORDONNANCEMENT");
            exit(EXIT_FAILURE);
            
        }
        sched_setattr(0, &ord, 0);           
    }

    // Écrivez le code permettant de redimensionner une image (en utilisant les fonctions précodées
    // dans utils.c, celles commençant par "resize"). Votre code doit lire une image depuis une zone 
    // mémoire partagée et envoyer le résultat sur une autre zone mémoire partagée.
    // N'oubliez pas de respecter la syntaxe de la ligne de commande présentée dans l'énoncé.

    // Memoire initialisation
    struct memPartage memImageIn, memImageOut;
    struct memPartageHeader memPH;

    initMemoirePartageeLecteur(imageIn,&memImageIn);

    memPH.frameWriter = 0;
    memPH.frameReader = 0;
    memPH.hauteur = hauteur;
    memPH.largeur = largeur;
    memPH.fps = memImageIn.header->fps;
    memPH.canaux = memImageIn.header->canaux;
    
    // Preparation memoire & initialisation memoire ecrivain
    prepareMemoire(memImageIn.tailleDonnees, hauteur*largeur*memImageIn.header->canaux);
    //initMemoirePartageeEcrivain(imageOut, &memImageOut);

    int hauteur_in = memImageIn.header->hauteur;
    int largeur_in = memImageIn.header->largeur;
    int canaux_in = memImageIn.header->canaux;

    // Redimensionner grille en fonction du mode choisi
    // Relacher memoire en lecture
    // Relacher memoire en ecriture
    // Attente signal ecrivain pour lire une modification
    // TODO




    return 0;
}