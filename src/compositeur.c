/*****************************************************************************
* H2020
* Compositeur, laboratoire 3, SETR
*
* Récupère plusieurs flux vidéos à partir d'espaces mémoire partagés et les
* affiche directement dans le framebuffer de la carte graphique.
* 
* IMPORTANT : CE CODE ASSUME QUE TOUS LES FLUX QU'IL REÇOIT SONT EN 427x240
* (427 pixels en largeur, 240 en hauteur). TOUTE AUTRE TAILLE ENTRAÎNERA UN
* COMPORTEMENT INDÉFINI. Les flux peuvent comporter 1 ou 3 canaux. Dans ce
* dernier cas, ils doivent être dans l'ordre BGR et NON RGB.
*
* Le code permettant l'affichage est inspiré de celui présenté sur le blog
* Raspberry Compote (http://raspberrycompote.blogspot.ie/2014/03/low-level-graphics-on-raspberry-pi-part_14.html),
* par J-P Rosti, publié sous la licence CC-BY 3.0.
*
* Marc-André Gardner, Février 2017. Mis à jour en février 2018.
* Merci à Yannick Hold-Geoffroy pour l'aide apportée pour la gestion
* du framebuffer.
******************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <getopt.h>
#include <stropts.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <sys/types.h>

// Allocation mémoire, mmap et mlock
#include <sys/mman.h>

// Gestion des ressources et permissions
#include <sys/resource.h>

// Mesure du temps
#include <time.h>

// Obtenir la taille des fichiers
#include <sys/stat.h>

// Contrôle de la console
#include <linux/fb.h>
#include <linux/kd.h>

// Gestion des erreurs
#include <err.h>
#include <errno.h>

// Nécessaire pour pouvoir utiliser sched_setattr et le mode DEADLINE
#include <sched.h>
#include "schedsupp.h"

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"

#define DEFAULT_FLUX_ENTREE "/chewy\0"
#define DEFAULT_ORDO "NORT\0"
#define DEFAULT_DEADLINE_OPTION "1000,1000,1000\0"
double frames_cnt[] = {0, 0, 0, 0};


// Fonction permettant de récupérer le temps courant sous forme double
double get_time()
{
	struct timeval t;
	struct timezone tzp;
	gettimeofday(&t, &tzp);
	return (double)t.tv_sec + (double)(t.tv_usec)*1e-6;
}


// Cette fonction écrit l'image dans le framebuffer, à la position demandée. Elle est déjà codée pour vous,
// mais vous devez l'utiliser correctement. En particulier, n'oubliez pas que cette fonction assume que
// TOUTES LES IMAGES QU'ELLE REÇOIT SONT EN 427x240 (1 ou 3 canaux). Cette fonction peut gérer
// l'affichage de 1, 2, 3 ou 4 images sur le même écran, en utilisant la séparation préconisée dans l'énoncé.
// La position (premier argument) doit être un entier inférieur au nombre total d'images à afficher (second argument).
// Le troisième argument est le descripteur de fichier du framebuffer (nommé fbfb dans la fonction main()).
// Le quatrième argument est un pointeur sur le memory map de ce framebuffer (nommé fbd dans la fonction main()).
// Les cinquième et sixième arguments sont la largeur et la hauteur de ce framebuffer.
// Le septième est une structure contenant l'information sur le framebuffer (nommé vinfo dans la fonction main()).
// Le huitième est la longueur effective d'une ligne du framebuffer (en octets), contenue dans finfo.line_length dans la fonction main().
// Le neuvième argument est le buffer contenant l'image à afficher, et les trois derniers arguments ses dimensions.
void ecrireImage(const int position, const int total,
					int fbfd, unsigned char* fb, size_t largeurFB, size_t hauteurFB, struct fb_var_screeninfo *vinfoPtr, int fbLineLength,
					const unsigned char *data, size_t largeurSource, size_t hauteurSource, size_t canauxSource){
	static int currentPage = 0;
	static unsigned char* imageGlobale = NULL;
	if(imageGlobale == NULL)
		imageGlobale = (unsigned char*)calloc(fbLineLength*hauteurFB, 1);

	currentPage = (currentPage+1) % 2;
	unsigned char *currentFramebuffer = fb + currentPage * fbLineLength * hauteurFB;

	if(position >= total){
		return;
	}

	const unsigned char *dataTraite = data;
	unsigned char* d = NULL;
	if(canauxSource == 1){
		d = (unsigned char*)tempsreel_malloc(largeurSource*hauteurSource*3);
		unsigned int pos = 0;
		for(unsigned int i=0; i < hauteurSource; ++i){
			for(unsigned int j=0; j < largeurSource; ++j){
				d[pos++] = data[i*largeurSource + j];
				d[pos++] = data[i*largeurSource + j];
				d[pos++] = data[i*largeurSource + j];
			}
		}
		dataTraite = d;
	}


	if(total == 1){
		// Une seule image en plein écran
		for(unsigned int ligne=0; ligne < hauteurSource; ligne++){
			memcpy(currentFramebuffer + ligne * fbLineLength, dataTraite + ligne * largeurSource * 3, largeurFB * 3);
		}
	}
	else if(total == 2){
		// Deux images
		if(position == 0){
			// Image du haut
			for(unsigned int ligne=0; ligne < hauteurSource; ligne++){
				memcpy(imageGlobale + ligne * fbLineLength, dataTraite + ligne * largeurSource * 3, largeurFB * 3);
			}
		}
		else{
			// Image du bas
			for(unsigned int ligne=hauteurSource; ligne < hauteurSource*2; ligne++){
				memcpy(imageGlobale + ligne * fbLineLength, dataTraite + (ligne-hauteurSource) * largeurSource * 3, largeurFB * 3);
			}
		}
	}
	else if(total == 3 || total == 4){
		// 3 ou 4 images
		off_t offsetLigne = 0;
		off_t offsetColonne = 0;
		switch (position) {
			case 0:
				// En haut, à gauche
				break;
			case 1:
				// En haut, à droite
				offsetColonne = largeurSource;
				break;
			case 2:
				// En bas, à gauche
				offsetLigne = hauteurSource;
				break;
			case 3:
				// En bas, à droite
				offsetLigne = hauteurSource;
				offsetColonne = largeurSource;
				break;
		}
		// On copie les données ligne par ligne
		offsetLigne *= fbLineLength;
		offsetColonne *= 3;
		for(unsigned int ligne=0; ligne < hauteurSource; ligne++){
			memcpy(imageGlobale + offsetLigne + offsetColonne, dataTraite + ligne * largeurSource * 3, largeurSource * 3);
			offsetLigne += fbLineLength;
		}
	}

	if(total > 1)
		memcpy(currentFramebuffer, imageGlobale, fbLineLength*hauteurFB);
        
        if(canauxSource == 1)
		tempsreel_free(d);
        
	vinfoPtr->yoffset = currentPage * vinfoPtr->yres;
	vinfoPtr->activate = FB_ACTIVATE_VBL;
	if (ioctl(fbfd, FBIOPAN_DISPLAY, vinfoPtr)) {
		printf("Erreur lors du changement de buffer (double buffering inactif)!\n");
	}

	frames_cnt[position]++;
}



int main(int argc, char* argv[])
{
    // TODO
    // ÉCRIVEZ ICI votre code d'analyse des arguments du programme et d'initialisation des zones mémoire partagées
	int debug = 0;
	struct memPartage zone[4];
    char type_ord[100] = DEFAULT_ORDO;
    char options[100] = DEFAULT_DEADLINE_OPTION ;
    struct sched_attr ord;
	int nbrActifs = 1;      // Après votre initialisation, cette variable DOIT contenir le nombre de flux vidéos actifs (de 1 à 4 inclusivement).
	size_t max_size = 0;
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
		nbrActifs = argc - optind;
        if (nbrActifs >= 1 && nbrActifs < 5) {

			for(int i = 0; i < nbrActifs; i++){
				printf("compositeur: in%i = %s \n",i+1,argv[i+optind]);
				if(initMemoirePartageeLecteur(argv[i+optind], &zone[i]) != 0)
					exit(EXIT_FAILURE);
				// Pour que le trylock dans attenteLecteurAsync ne produise pas de deadlock	
				pthread_mutex_unlock(&zone[i].header->mutex);
				if (zone[i].tailleDonnees > max_size) 
					max_size = zone[i].tailleDonnees;
			}
		} else{
			fprintf(stderr, "Le compositeur a besoin d'au moins un flux d'entree et un maximum de 4 flux d'entree\n");
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
            printf("NO BUENO ORDONNANCEMENT \n");
            exit(EXIT_FAILURE);
        }
        sched_setattr(0, &ord, 0);

    }
	else{
		if( initMemoirePartageeLecteur(DEFAULT_FLUX_ENTREE, &zone[0]) != 0)
			exit(EXIT_FAILURE);	
		// Pour que le trylock dans attenteLecteurAsync ne produise pas de deadlock	
		pthread_mutex_unlock(&zone[0].header->mutex);	
	}



	prepareMemoire(5*max_size, 0);

	/*
	* END OF MY CODE
	*/
   
    // Initialisation des structures nécessaires à l'affichage
    long int screensize = 0;
    // Ouverture du framebuffer
    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        perror("Erreur lors de l'ouverture du framebuffer ");
        return -1;
    }

    // Obtention des informations sur l'affichage et le framebuffer
    struct fb_var_screeninfo vinfo;
    struct fb_var_screeninfo orig_vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Erreur lors de la requete d'informations sur le framebuffer ");
    }

    // On conserve les précédents paramètres
    memcpy(&orig_vinfo, &vinfo, sizeof(struct fb_var_screeninfo));

    // On choisit la bonne résolution
    vinfo.bits_per_pixel = 24;
	switch (nbrActifs) {
		case 1:
			vinfo.xres = 427;
			vinfo.yres = 240;
			break;
		case 2:
			vinfo.xres = 427;
			vinfo.yres = 480;
			break;
		case 3:
		case 4:
			vinfo.xres = 854;
			vinfo.yres = 480;
			break;
		default:
			printf("Nombre de sources invalide!\n");
			return -1;
			break;
	}

    vinfo.xres_virtual = vinfo.xres;
    vinfo.yres_virtual = vinfo.yres * 2;
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo)) {
		perror("Erreur lors de l'appel a ioctl ");
    }

    // On récupère les "vraies" paramètres du framebuffer
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
		perror("Erreur lors de l'appel a ioctl (2) ");
    }

    // On fait un mmap pour avoir directement accès au framebuffer
    screensize = finfo.smem_len;
    unsigned char *fbp = (unsigned char*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

    if (fbp == MAP_FAILED) {
		perror("Erreur lors du mmap de l'affichage ");
		return -1;
    }

	/*
	* MY CODE HERE
	*/
	double framePeriode[4];
	double lastFrameTime[4];
	double currentTime = get_time();
	double lastFPSPooling = currentTime;
	double deltaTime = 0;

	for(int i = 0; i < nbrActifs; i++){
		framePeriode[i] = (double) (1.0 / zone[i].header->fps);
		lastFrameTime[i] = get_time();
	}

	FILE *fileStat = fopen("stats.txt", "w+");
	fclose(fileStat);
    while(1){
            // Boucle principale du programme
            // TODO
            // Appelez ici ecrireImage() avec les images provenant des différents flux vidéo
            // Attention à ne pas mélanger les flux, et à ne pas bloquer sur un mutex (ce qui
            // bloquerait l'interface entière)
            // Nous vous conseillons d'implémenter une limitation du nombre de FPS (images par
            // seconde), nombre qui est spécifié pour chaque flux. Il est inutile d'aller plus
            // vite que le nombre de FPS demandé, et cela consomme plus de ressources, ce qui
            // peut rendre plus difficile l'exécution des configurations difficiles.
        
            // N'oubliez pas que toutes les images fournies à ecrireImage() DOIVENT être en
            // 427x240 (voir le commentaire en haut du document).
        
            // Exemple d'appel à ecrireImage (n'oubliez pas de remplacer les arguments commençant par A_REMPLIR!)

			// Ecrit le FPS de tous les flux dans le fichier stat.txt
			currentTime = get_time();
			deltaTime = currentTime - lastFPSPooling;
			if(deltaTime > 1){ //update the fps count in stat.txt

				fileStat = fopen("stats.txt", "a");
				if (fileStat == NULL) {
					printf("Erreur a l'ouverture du fichier de log.\n");
				}

				fseek(fileStat, 0, SEEK_END);
				for (int i = 0; i < nbrActifs; i++) {
					fprintf(fileStat, "(%f) flux %d: %f\n", currentTime, i+1, frames_cnt[i] / deltaTime);
					frames_cnt[i] = 0;
            	}
				fclose(fileStat);
				lastFPSPooling = currentTime;
			}

			// Rafraichi l'image de chaque flux sur lecran si celui-ci est du a etre rafrachire
			// en fonction de son targeted FPS
			for(int i = 0; i < nbrActifs; i++){

				if(currentTime - lastFrameTime[i] > framePeriode[i]){
					if(attenteLecteurAsync(&zone[i]) == 0){		
						uint32_t largeur = zone[i].header->largeur;
						uint32_t hauteur = zone[i].header->hauteur;
						uint32_t canaux = zone[i].header->canaux;
						ecrireImage(i, 
									nbrActifs, 
									fbfd, 
									fbp, 
									vinfo.xres, 
									vinfo.yres, 
									&vinfo, 
									finfo.line_length,
									zone[i].data,
									largeur,
									hauteur,
									canaux);		
						zone[i].header->frameReader += 1;
						zone[i].copieCompteur = zone[i].header->frameWriter;						
						lastFrameTime[i] = currentTime;											
						pthread_mutex_unlock(&zone[i].header->mutex);
					}
				}
			}
    }


    // cleanup
    // Retirer le mmap
    munmap(fbp, screensize);


    // reset the display mode
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo)) {
        printf("Error re-setting variable information.\n");
    }
    // Fermer le framebuffer
    close(fbfd);

    return 0;

}

