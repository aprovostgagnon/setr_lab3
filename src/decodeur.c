// Gestion des ressources et permissions
#include <sys/resource.h>

// Nécessaire pour pouvoir utiliser sched_setattr et le mode DEADLINE
#include <sched.h>
#include "schedsupp.h"

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"
#include <getopt.h>
#include "jpgd.h"

// Définition de diverses structures pouvant vous être utiles pour la lecture d'un fichier ULV
#define HEADER_SIZE 4
#define OFFSET_FIRST_IMAGE 20
#define DEFAULT_ORDO "NORT\0"
#define DEFAULT_DEADLINE_OPTION "1000,1000,1000\0"
#define VIDEO_PATH   "/home/pi/projects/laboratoire3/480p/02_Sintel.ulv"
#define MEMOIRE_SETR   "/peanut\0"
const char header[] = "SETR";

struct videoInfos{
        uint32_t largeur;
        uint32_t hauteur;
        uint32_t canaux;
        uint32_t fps;
};

/******************************************************************************
* FORMAT DU FICHIER VIDEO
* Offset     Taille     Type      Description
* 0          4          char      Header (toujours "SETR" en ASCII)
* 4          4          uint32    Largeur des images du vidéo
* 8          4          uint32    Hauteur des images du vidéo
* 12         4          uint32    Nombre de canaux dans les images
* 16         4          uint32    Nombre d'images par seconde (FPS)
* 20         4          uint32    Taille (en octets) de la première image -> N
* 24         N          char      Contenu de la première image (row-first)
* 24+N       4          uint32    Taille (en octets) de la seconde image -> N2
* 24+N+4     N2         char      Contenu de la seconde image
* 24+N+N2    4          uint32    Taille (en octets) de la troisième image -> N2
* ...                             Toutes les images composant la vidéo, à la suite
*            4          uint32    0 (indique la fin du fichier)
******************************************************************************/


int main(int argc, char* argv[]){
    int debug = 0;
    char flux_entree[100]= VIDEO_PATH;
    char flux_sortie[100] = MEMOIRE_SETR;
    char type_ord[100] = DEFAULT_ORDO;
    char options[100] = DEFAULT_DEADLINE_OPTION ;
    struct sched_attr ord;
    int c;
    int option_index = 0;

    static struct option long_options[] = {
            {"debug", no_argument, 0,  'a'}
    };
    while ((c = getopt_long(argc, argv, "a::d::s:",long_options,&option_index)) != -1) {
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
            strcpy(flux_entree,argv[optind]);
            strcpy(flux_sortie,argv[optind+1]);
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
    

    // Écrivez le code de décodage et d'envoi sur la zone mémoire partagée ici!
    // N'oubliez pas que vous pouvez utiliser jpgd::decompress_jpeg_image_from_memory()
    // pour décoder une image JPEG contenue dans un buffer!
    // N'oubliez pas également que ce décodeur doit lire les fichiers ULV EN BOUCLE
    
    int fileInfo = open(flux_entree,O_RDONLY);
    if  (fileInfo < 0){

        printf("OPEN WRONG file : %s\n",flux_entree );
        exit(EXIT_FAILURE);
    }
    // Copier de tout le contenu fichier avec mmap
    // recuperer informations pertinentes à passer a mmap
    // int fstat(int fildes, struct stat *buf);
    // stat.st_size
    // void *mmap(void *addr, size_t length, int prot, int flags,int fd, off_t offset);
    struct stat statusVideo;
    fstat (fileInfo, &statusVideo);
    const char *videoInMem = (char*)mmap(NULL, statusVideo.st_size, PROT_READ,MAP_POPULATE | MAP_PRIVATE, fileInfo, 0);

    if(strncmp(header, videoInMem, 4) != 0){
        printf("wrong video format\n");
        exit(1);
    }

    // Copier informations contenu dans format ULV
    // void * memcpy ( void * destination, const void * source, size_t num );
    videoInfos vInfos;
    memcpy(&vInfos.largeur,videoInMem+4 ,4);
    memcpy(&vInfos.hauteur,videoInMem+8 ,4);
    memcpy(&vInfos.canaux, videoInMem+12,4);
    memcpy(&vInfos.fps, videoInMem+16,4);
    size_t tailleFenetre = vInfos.largeur * vInfos.hauteur * vInfos.canaux;
    prepareMemoire(0, 10*tailleFenetre);

    // Memoire initialisation
    memPartage memP;
    memPartageHeader memPH;
    memPH.frameWriter = 0;
    memPH.frameReader = 0;
    memPH.hauteur = vInfos.hauteur;
    memPH.largeur = vInfos.largeur;
    memPH.canaux = vInfos.canaux;
    memPH.fps = vInfos.fps;

    initMemoirePartageeEcrivain(flux_sortie, &memP, tailleFenetre, &memPH);
    // Copie du fichier en memoire
    // On se place a un offset de 20 par rapport au debut du fichier
    
    uint32_t index = OFFSET_FIRST_IMAGE;
    while (1){

        if (index >= (uint32_t) statusVideo.st_size - 4) { // Verification si on a depasse la derniere frame
            index = OFFSET_FIRST_IMAGE;// retourne au debut de la video (premiere frame)
        }

        // PROCHAINE IMAGE
        uint32_t tailleImageCompress;
        memcpy(&tailleImageCompress, videoInMem + index, 4); // copie taille image
        index +=4;



        //unsigned char *decompress_jpeg_image_from_memory
        //(const unsigned char *pSrc_data, int src_data_size, 
        //int *width, int *height, int *actual_comps, int req_comps)
        int larg = vInfos.largeur;
        int haut = vInfos.hauteur;
        int comp = 0;

        unsigned char* fenetre = 
        jpgd::decompress_jpeg_image_from_memory((const unsigned char*)(videoInMem+index), tailleImageCompress,
        &larg, &haut,&comp,vInfos.canaux);

        // informations memoire partagee MAJ
        memP.header->largeur = larg;
        memP.header->hauteur = haut;
        memP.header->canaux = comp;

        if ((larg != (int)vInfos.largeur) || (haut != (int)vInfos.hauteur) || (comp != (int)vInfos.canaux)){
            printf("UNA COZA NO BUENO\n");
            exit(EXIT_FAILURE);
        }

        // On peut copier l'image puis liberer la memoire
        // Calcul de la taille
        uint32_t taille = larg*haut*comp;
        memcpy(memP.data, fenetre, taille);
        tempsreel_free(fenetre);

        // Liberer mutex pthread_mutex_t mutex;
        // Incrementer index ecrivain
        memP.header->frameWriter++;
        memP.copieCompteur = memP.header->frameReader;
        index += tailleImageCompress;
        pthread_mutex_unlock(&memP.header->mutex);
        // Attendre que le lecteur lit une frame et libere le mutex
        attenteEcrivain(&memP); 
    }


    return 0;
}

