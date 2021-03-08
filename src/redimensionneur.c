// Gestion des ressources et permissions
#include <sys/resource.h>

// Nécessaire pour pouvoir utiliser sched_setattr et le mode DEADLINE
#include <sched.h>
#include "schedsupp.h"

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"
#include <getopt.h>
#define DEFAULT_ORDO "NORT"
#define DEFAULT_DEADLINE_OPTION "1000,1000,1000"
#define DEFAULT_IMAGE_IN "/peanut"
#define DEFAULT_IMAGE_OUT "/chewy"

int main(int argc, char* argv[]){
    

        int debug = 0;
        char imageIn[100] = DEFAULT_IMAGE_IN;
        char imageOut [100] = DEFAULT_IMAGE_OUT;
        char type_ord[100] = DEFAULT_ORDO;
        char options[100] = DEFAULT_DEADLINE_OPTION ;
        int largeur = 427;
        int hauteur = 240;
        int methode = 0;
        struct sched_attr ord;
        int c;
        int option_index = 0;

        static struct option long_options[] = {
                {"debug", no_argument, 0,  'a'}
        };

        while ((c = getopt_long(argc, argv, "aw:h:s:d::m:",long_options,&option_index)) != -1) {
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
        if (argc - optind  == 2) {
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
    prepareMemoire(5*memImageIn.tailleDonnees, 5*hauteur*largeur*memImageIn.header->canaux);

    // Creer la memoire partage out en fonction de l'agrandissement voulut
    size_t sizeOut = largeur * hauteur * memImageIn.header->canaux;
    if(initMemoirePartageeEcrivain(imageOut, &memImageOut,sizeOut,&memPH) != 0)
        exit(1);
    printf("redimensionneur: init mem ecrivain \n");
    unsigned int hauteur_in = (unsigned int) memImageIn.header->hauteur;
    unsigned int largeur_in = (unsigned int) memImageIn.header->largeur;
    unsigned int canaux_in = (unsigned int) memImageIn.header->canaux;

    // Redimensionner grille en fonction du mode choisi
    ResizeGrid res;
    if (methode == 0) {
        res= resizeNearestNeighborInit(hauteur, largeur, hauteur_in, largeur_in);
    }  else if (methode== 1) {
        res= resizeBilinearInit(hauteur, largeur, hauteur_in, largeur_in);
    }  else {
        fprintf(stderr, "not good methode\n");
        exit(EXIT_FAILURE);
    }    
    // Relacher memoire en lecture
    // Relacher memoire en ecriture
    // Attente signal ecrivain pour lire une modification
    // TODO
    while(1){
        
        if (methode == 0) {
            resizeNearestNeighbor(memImageIn.data, hauteur_in, largeur_in, memImageOut.data, hauteur, largeur ,res, canaux_in);
        }
        else {
            resizeBilinear(memImageIn.data, hauteur_in, largeur_in, memImageOut.data, hauteur, largeur, res, canaux_in);
        }
        
        memImageIn.copieCompteur = memImageIn.header->frameWriter;
        memImageIn.header->frameReader++;
        pthread_mutex_unlock(&memImageIn.header->mutex);

        memImageOut.copieCompteur = memImageOut.header->frameReader;
        memImageOut.header->frameWriter++;
        pthread_mutex_unlock(&memImageOut.header->mutex);

        attenteLecteur(&memImageIn);
        

        attenteEcrivain(&memImageOut);

    }



    return 0;
}