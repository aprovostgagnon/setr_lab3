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
#define DEFAULT_IMAGE_IN "/greyIn"
#define DEFAULT_IMAGE_OUT "/greyOut"

int main(int argc, char* argv[]){
    
    // Écrivez le code permettant de convertir une image en niveaux de gris, en utilisant la
    // fonction convertToGray de utils.c. Votre code doit lire une image depuis une zone mémoire 
    // partagée et envoyer le résultat sur une autre zone mémoire partagée.
    // N'oubliez pas de respecter la syntaxe de la ligne de commande présentée dans l'énoncé.

        int debug = 0;
        char imageIn[100] = DEFAULT_IMAGE_IN;
        char imageOut [100] = DEFAULT_IMAGE_OUT;
        char type_ord[100] = DEFAULT_ORDO;
        char options[100] = DEFAULT_DEADLINE_OPTION ;
        struct sched_attr ord;
        int c;
        int option_index = 0;

        static struct option long_options[] = {
                {"debug", no_argument, 0,  'a'}
        };

        while ((c = getopt_long(argc, argv, "as:d::",long_options,&option_index)) != -1) {
        switch (c) {

        case 'a': 
            debug = 1; 
            break;

        case 'd': 
            strcpy(options, optarg);
            break;

        case 's':
            strcpy(type_ord, optarg);
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


    // Memoire initialisation
    struct memPartage memImageIn, memImageOut;

    //init memoire lecteur. la memoire est lock a la fin
    initMemoirePartageeLecteur(imageIn,&memImageIn);

    uint32_t w = memImageIn.header->largeur;
    uint32_t h = memImageIn.header->hauteur;
    uint32_t ch = memImageIn.header->canaux;
    

    memPartageHeader memPH;
    memPH.frameWriter = 0;
    memPH.frameReader = 0;
    memPH.hauteur = memImageIn.header->hauteur;
    memPH.largeur = memImageIn.header->largeur;
    memPH.canaux = 1;
    memPH.fps = memImageIn.header->fps;
    size_t tailleDonnees = w*h*1;

    // Preparation memoire & initialisation memoire ecrivain
    prepareMemoire(20*w*h*ch, 20*tailleDonnees);

    // Creer la memoire partage  qui sera filtre. la memoire est lock a la fin 
    if(initMemoirePartageeEcrivain(imageOut, &memImageOut,tailleDonnees,&memPH) != 0)
        exit(1);

    while(1){

        //passe le filtre de couleur
        convertToGray(memImageIn.data, h, w,ch, memImageOut.data);

        //libere les memoires partage
        memImageIn.copieCompteur = memImageIn.header->frameWriter;
        memImageIn.header->frameReader++;
        pthread_mutex_unlock(&memImageIn.header->mutex);

        memImageOut.copieCompteur = memImageOut.header->frameReader;
        memImageOut.header->frameWriter++;
        pthread_mutex_unlock(&memImageOut.header->mutex);

        // reprend le control des memoires pour une autre iteration
        
        attenteLecteur(&memImageIn);
        attenteEcrivain(&memImageOut);
    }
    return 0;
}