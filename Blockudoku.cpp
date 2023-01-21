#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include "GrilleSDL.h"
#include "Ressources.h"

// Dimensions de la grille de jeu
#define NB_LIGNES   12
#define NB_COLONNES 19

// Nombre de cases maximum par piece
#define NB_CASES    4

// Macros utilisees dans le tableau tab
#define VIDE        0
#define BRIQUE      1
#define DIAMANT     2

int tab[NB_LIGNES][NB_COLONNES] ={
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

typedef struct {
	int ligne;
	int colonne;
} CASE;

typedef struct {
	CASE cases[NB_CASES];
	int  nbCases;
	int  couleur;
} PIECE;

PIECE pieces[12] = { 0,0,0,1,1,0,1,1,4,0,       // carre 4
                     0,0,1,0,2,0,2,1,4,0,       // L 4
                     0,1,1,1,2,0,2,1,4,0,       // J 4
                     0,0,0,1,1,1,1,2,4,0,       // Z 4
                     0,1,0,2,1,0,1,1,4,0,       // S 4
                     0,0,0,1,0,2,1,1,4,0,       // T 4
                     0,0,0,1,0,2,0,3,4,0,       // I 4
                     0,0,0,1,0,2,0,0,3,0,       // I 3
                     0,1,1,0,1,1,0,0,3,0,       // J 3
                     0,0,1,0,1,1,0,0,3,0,       // L 3
                     0,0,0,1,0,0,0,0,2,0,       // I 2
                     0,0,0,0,0,0,0,0,1,0 };     // carre 1

char* message;     // pointeur vers le message à faire défiler
int tailleMessage; // longueur du message
int indiceCourant; // indice du premier caractère à afficher dans la zone graphique
pthread_t pthreadMessage;
pthread_mutex_t mutexMessage;

PIECE pieceEnCours;
CASE casesInserees[NB_CASES];
int nbCasesInserees;
pthread_t pthreadPiece;
pthread_mutex_t mutexCasesInserees;
pthread_cond_t condCasesInserees;

pthread_t pthreadEvent;

int score;
bool MAJScore;
pthread_t pthreadScore;
pthread_mutex_t mutexScore;
pthread_cond_t condScore;

int lignesCompletes[NB_CASES];
int nbLignesCompletes;
int colonnesCompletes[NB_CASES];
int nbColonnesCompletes;
int carresComplets[NB_CASES];
int nbCarresComplets;
int nbAnalyses;
pthread_t tabThreadCase[9][9];
pthread_mutex_t mutexAnalyse;
pthread_key_t cleCase;

int combos;
bool MAJCombos;
pthread_t pthreadNettoyeur;
pthread_cond_t condAnalyse;

bool traitementEnCours;
pthread_mutex_t mutexTraitement;
pthread_cond_t condTraitement;

int  CompareCases(CASE case1,CASE case2);
void TriCases(CASE* vecteur, int indiceDebut, int indiceFin);

void threadDefileMessage();
void setMessage(const char*, bool);
void setMessageBase(int);
void EndMessage();

void threadPiece();
void RotationPiece(PIECE * pPiece);

void threadEvent();

void threadScore();

void threadCases(CASE*);
void handlerSIGUSR1(int);
void EndCases(void*);

void threadNettoyeur();


///////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc,char* argv[]) {
	EVENT_GRILLE_SDL event;
	int croix;
	struct timespec timer;
	timer.tv_nsec = 100000;
	srand((unsigned)time(NULL));
	sigset_t mask;
	sigfillset(&mask);
	sigprocmask(SIG_SETMASK, &mask, NULL);
	
	pthread_mutex_init(&mutexMessage, NULL);
	pthread_mutex_init(&mutexCasesInserees, NULL);
	pthread_mutex_init(&mutexAnalyse, NULL);
	pthread_cond_init(&condCasesInserees, NULL);
	pthread_key_create(&cleCase, (void(*)(void*))EndCases);
	pthread_cond_init(&condAnalyse, NULL);
	pthread_mutex_init(&mutexTraitement, NULL);
	pthread_cond_init(&condTraitement, NULL);
	// Ouverture de la fenetre graphique
	printf("(MAIN %d) Ouverture de la fenetre graphique\n",pthread_self()); fflush(stdout);
	if (OuvertureFenetreGraphique() < 0) 
	{
		printf("Erreur de OuvrirGrilleSDL\n");
		fflush(stdout);
		exit(1);
	}
	setMessage("Bienvenue dans Blockudoku", true);
	pthread_create(&pthreadMessage, NULL, (void*(*)(void*))threadDefileMessage, NULL);
	pthread_detach(pthreadMessage);

	pthread_create(&pthreadPiece, NULL, (void*(*)(void*))threadPiece, NULL);
	pthread_detach(pthreadPiece);

	pthread_create(&pthreadScore, NULL, (void*(*)(void*))threadScore, NULL);
	pthread_detach(pthreadScore);

	pthread_create(&pthreadEvent, NULL, (void*(*)(void*))threadEvent, NULL);
	CASE* paramCase;
	for (int L=0;L<9;L++) {
		for (int C=0;C<9;C++) {
			paramCase = (CASE*)malloc(sizeof(CASE));
			paramCase->ligne = L;
			paramCase->colonne = C;
			pthread_create(&tabThreadCase[L][C], NULL, (void*(*)(void*))threadCases, paramCase);
			// pthread_detach(tabThreadCase[L][C]);
		}
	}
	pthread_create(&pthreadNettoyeur, NULL, (void*(*)(void*))threadNettoyeur, NULL);
	pthread_detach(pthreadNettoyeur);
	
	DessineVoyant(8, 10, VERT);

	//attente de la fin du jeu
	pthread_join(pthreadEvent, NULL);

	pthread_cancel(pthreadPiece);

	//destruction des pthreadCase
	for (int L=0;L<9;L++) {
		for (int C=0;C<9;C++) {
			pthread_cancel(tabThreadCase[L][C]);
			pthread_join(tabThreadCase[L][C], NULL);
		}
	}
	// pthread_key_delete(cleCase);

	//destruction du pthreadMessage
	pthread_cancel(pthreadMessage);

	// Fermeture de la fenetre
	nanosleep(&timer, NULL);
	printf("(MAIN %d) Fermeture de la fenetre graphique...",pthread_self()); fflush(stdout);
	FermetureFenetreGraphique();
	printf("OK\n");

	exit(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int CompareCases(CASE case1,CASE case2) {
	if (case1.ligne < case2.ligne) return -1;
	if (case1.ligne > case2.ligne) return +1;
	if (case1.colonne < case2.colonne) return -1;
	if (case1.colonne > case2.colonne) return +1;
	return 0;
}
void TriCases(CASE *vecteur,int indiceDebut,int indiceFin) {
	// trie les cases de vecteur entre les indices indiceDebut et indiceFin compris
	// selon le critere impose par la fonction CompareCases()
	// Exemple : pour trier un vecteur v de 4 cases, il faut appeler TriCases(v,0,3); 
	int  i,iMin;
	CASE tmp;

	if (indiceDebut >= indiceFin) return;

	// Recherche du minimum
	iMin = indiceDebut;
	for (i=indiceDebut ; i<=indiceFin ; i++)
		if (CompareCases(vecteur[i],vecteur[iMin]) < 0) iMin = i;

	// On place le minimum a l'indiceDebut par permutation
	tmp = vecteur[indiceDebut];
	vecteur[indiceDebut] = vecteur[iMin];
	vecteur[iMin] = tmp;

	// Tri du reste du vecteur par recursivite
	TriCases(vecteur,indiceDebut+1,indiceFin); 
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void threadDefileMessage() {
	struct timespec timer;
	struct sigaction s;
	pthread_cleanup_push((void(*)(void*))EndMessage, 0);
	printf("(thread %u) Demarrage de la file de message\n", pthread_self());
	timer.tv_nsec = 400000000;	//0.4s
	s.sa_handler = setMessageBase;
	sigfillset(&s.sa_mask);
	sigdelset(&s.sa_mask, SIGALRM);
	sigprocmask(SIG_SETMASK, &s.sa_mask, NULL);
	s.sa_flags = 0;
	sigaction(SIGALRM, &s, NULL);
	while(1) {
		pthread_mutex_lock(&mutexMessage);
		for (int i=0;i<17;i++) {
			DessineLettre(10, 1+i, *(message + (i+indiceCourant)%tailleMessage));
		}
		pthread_mutex_unlock(&mutexMessage);
		indiceCourant++;
		nanosleep(&timer, NULL);
	}
	pthread_cleanup_pop(0);
}
void setMessage(const char* texte, bool signalOn) {
	alarm(0);		//on annule un possible signal precedent
	if (signalOn) alarm(10);

	pthread_mutex_lock(&mutexMessage);

	tailleMessage = strlen(texte) + 4;	//on rajoute 3 espace apres la chaine
	free(message);		//si en cas une chaine existe deja
	message = (char*)malloc(tailleMessage);
	strcpy(message, texte);
	for (int i=1;i<5;i++)
		message[tailleMessage-i] = ' ';

	pthread_mutex_unlock(&mutexMessage);

}
void setMessageBase(int sig) {
	setMessage("Jeu en cours", false);
}
void EndMessage() {
	printf("(thread %u) Fin de la file de message\n", pthread_self());
	free(message);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void threadPiece() {
	int rota;
	bool ko = true;
	int Lmin;
	int Cmin;
	CASE tmpCases[NB_CASES];
	printf("(thread %u) Demarrage du threadPiece\n", pthread_self());
	while (1) {
		srand(time(NULL));
		//genere la couleur et la piece
		int couleurs[4] = {JAUNE, ROUGE, VERT, VIOLET};
		rota = rand()%4;
		// pieceEnCours = pieces[rand()%12];
		pieceEnCours = pieces[0]; //c'est un carré pour test
		pieceEnCours.couleur = couleurs[rand()%4];
		for (int i=0;i<rota;i++) {	//effecteur les rotation
			RotationPiece(&pieceEnCours);
			TriCases(pieceEnCours.cases, 0, pieceEnCours.nbCases-1);
		}
		for (int i=0;i<pieceEnCours.nbCases;i++) {	//dessine la piece a droite
			DessineDiamant(3 + pieceEnCours.cases[i].ligne, 14+pieceEnCours.cases[i].colonne, pieceEnCours.couleur);
		}
		//verification si on peut placer la piece
		int LMax, CMax=0;
		LMax = pieceEnCours.cases[pieceEnCours.nbCases-1].ligne;
		for (int i=0;i<pieceEnCours.nbCases;i++) {
			if (pieceEnCours.cases[i].colonne > CMax) CMax = pieceEnCours.cases[i].colonne;
		}
		bool place = false;
		for (int i=0;i<9-LMax && !place;i++) {
			for (int j=0;j<9-CMax && !place;j++) {
				place = true;
				for(int h=0;h<pieceEnCours.nbCases&&place;h++) {
					if (tab[i+pieceEnCours.cases[h].ligne][j+pieceEnCours.cases[h].colonne] != VIDE) place = false;
				}
			}
		}
		if (!place) {	//fin du jeu
			setMessage("Game Over", false);
			pthread_mutex_lock(&mutexTraitement);
			traitementEnCours = true;
			pthread_mutex_unlock(&mutexTraitement);
			pthread_exit(0);
		}



		do {	//boucle qui tourne tant que la piece n'est pas inseree correctement
			pthread_mutex_lock(&mutexCasesInserees);
			for (int i=0;i<nbCasesInserees;i++) {	//efface tout le tableau en cas de mauvaise piece
				EffaceCarre(casesInserees[i].ligne, casesInserees[i].colonne);
				tab[casesInserees[i].ligne][casesInserees[i].colonne] = 0;
				casesInserees[i].ligne = 0;
				casesInserees[i].colonne = 0;
			}
			nbCasesInserees = 0;
			while (nbCasesInserees < pieceEnCours.nbCases) pthread_cond_wait(&condCasesInserees, &mutexCasesInserees);
			Lmin = casesInserees[0].ligne;
			Cmin = casesInserees[0].colonne;
			for (int i=0;i<nbCasesInserees;i++) {	// tri la piece insere afin de la comparer a la piece actuelle
				if (casesInserees[i].ligne < Lmin) Lmin = casesInserees[i].ligne;
				if (casesInserees[i].colonne < Cmin) Cmin = casesInserees[i].colonne;
			}
			for (int i=0;i<nbCasesInserees;i++) {
				tmpCases[i].ligne = casesInserees[i].ligne - Lmin;
				tmpCases[i].colonne = casesInserees[i].colonne - Cmin;
			}
			TriCases(tmpCases, 0, nbCasesInserees-1);
			ko = false;
			for (int i=0;i<nbCasesInserees;i++) {	//verifie que les pieces correspondent
				if (tmpCases[i].ligne != pieceEnCours.cases[i].ligne) ko = true;
				if (tmpCases[i].colonne != pieceEnCours.cases[i].colonne) ko = true;
			}
			pthread_mutex_unlock(&mutexCasesInserees);
		} while(ko);	//verifier les carre mis correspondent a la piece
		pthread_mutex_lock(&mutexTraitement);
		traitementEnCours = true;
		DessineVoyant(8, 10, BLEU);
		pthread_mutex_unlock(&mutexTraitement);
		pthread_mutex_lock(&mutexCasesInserees);
		for (int i=0;i<pieceEnCours.nbCases;i++) {
			tab[casesInserees[i].ligne][casesInserees[i].colonne] = BRIQUE;
			DessineBrique(casesInserees[i].ligne, casesInserees[i].colonne, false);
			pthread_kill(tabThreadCase[casesInserees[i].ligne][casesInserees[i].colonne], SIGUSR1);
		}
		for (int i=0;i<pieceEnCours.nbCases;i++) {		//efface la piece dans le cube a droite
			for (int j=0;j<4;j++) {
				EffaceCarre(3+i,14+j);
			}
		}
		pthread_mutex_lock(&mutexScore);
		score += nbCasesInserees;
		MAJScore = true;
		pthread_mutex_unlock(&mutexScore);
		pthread_cond_signal(&condScore);
		for (int i=0;i<nbCasesInserees;i++) {
			casesInserees[i].ligne = 0;
			casesInserees[i].colonne = 0;
		}
		nbCasesInserees = 0;
		pthread_mutex_unlock(&mutexCasesInserees);
		pthread_mutex_lock(&mutexTraitement);
		while(traitementEnCours) pthread_cond_wait(&condTraitement, &mutexTraitement);
		pthread_mutex_unlock(&mutexTraitement);
		DessineVoyant(8, 10, VERT);
	}
	printf("(thread %u) fin du threadPiece\n", pthread_self());

}
void RotationPiece(PIECE * pPiece) {
	int Lmin = 0;
	int tmp;
	for (int i=0;i<pPiece->nbCases;i++) {
		tmp = pPiece->cases[i].ligne;
		pPiece->cases[i].ligne = -(pPiece->cases[i].colonne);
		pPiece->cases[i].colonne = tmp;
		if (pPiece->cases[i].ligne < Lmin) Lmin = pPiece->cases[i].ligne;
	}
	if (Lmin != 0) {
		for (int i=0;i<pPiece->nbCases;i++) {
			pPiece->cases[i].ligne -= Lmin;
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void threadEvent() {
	EVENT_GRILLE_SDL event;
	struct timespec timer;
	timer.tv_nsec = 400000000;	//0.4s
	sigset_t mask;
	sigfillset(&mask);
	sigprocmask(SIG_SETMASK, &mask, NULL);

	printf("(thread %u) Demarrage du threadEvent\n", pthread_self());
	do {
		event = ReadEvent();
		if (event.type == CLIC_GAUCHE) {
			pthread_mutex_lock(&mutexTraitement);
			if (!tab[event.ligne][event.colonne] && event.ligne < 9 && event.colonne < 9 && !traitementEnCours) {
				pthread_mutex_lock(&mutexCasesInserees);
				DessineDiamant(event.ligne,event.colonne,pieceEnCours.couleur);
				tab[event.ligne][event.colonne] = DIAMANT;
				casesInserees[nbCasesInserees].ligne = event.ligne;
				casesInserees[nbCasesInserees].colonne = event.colonne;
				nbCasesInserees++;
				pthread_cond_signal(&condCasesInserees);
				pthread_mutex_unlock(&mutexCasesInserees);
			}
			else{
				DessineVoyant(8, 10, ROUGE);
				nanosleep(&timer, NULL);
				if (traitementEnCours) DessineVoyant(8, 10, BLEU);
				else DessineVoyant(8, 10, VERT);
			}
			pthread_mutex_unlock(&mutexTraitement);
		}
		if (event.type == CLIC_DROIT) {
			pthread_mutex_lock(&mutexCasesInserees);
			for (int i=0;i<nbCasesInserees;i++) {
				EffaceCarre(casesInserees[i].ligne, casesInserees[i].colonne);
				tab[casesInserees[i].ligne][casesInserees[i].colonne] = 0;
				casesInserees[i].ligne = 0;
				casesInserees[i].colonne = 0;
			}
			nbCasesInserees = 0;
			pthread_mutex_unlock(&mutexCasesInserees);
		}
	} while (event.type != CROIX);

	printf("(thread %u) Fermeture du ThreadEvent\n", pthread_self());
	pthread_exit(0);

}
///////////////////////////////////////////////////////////////////////////////////////////////////
void threadScore() {
	sigset_t mask;
	sigfillset(&mask);
	sigprocmask(SIG_SETMASK, &mask, NULL);
	int tmpScore;
	int tempCombos;
	printf("(thread %u) Demarrage du threadScore\n", pthread_self());
	while (1) {
		pthread_mutex_lock(&mutexScore);
		while(!MAJScore && !MAJCombos) pthread_cond_wait(&condScore, &mutexScore);
		pthread_mutex_unlock(&mutexScore);
		tmpScore = score;
		tempCombos = combos;
		for (int i=0;i<4;i++) 
		{
			DessineChiffre(1, 17-i, tmpScore%10);
			DessineChiffre(8, 17-i, tempCombos%10);
			tmpScore /= 10;
			tempCombos/= 10;
		}
		
		MAJScore = false;
		MAJCombos = false;
	}
	printf("(thread %u) Fin du threadScore\n", pthread_self());
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void threadCases(CASE* param) {
	struct sigaction s;
	s.sa_handler = handlerSIGUSR1;
	sigfillset(&s.sa_mask);
	sigdelset(&s.sa_mask, SIGUSR1);
	sigprocmask(SIG_SETMASK, &s.sa_mask, NULL);
	s.sa_flags = 0;
	sigaction(SIGUSR1, &s, NULL);
	CASE empl = {param->ligne, param->colonne};
	free(param);
	pthread_setspecific(cleCase, &empl);

	while(1) {
		pause();
		pthread_mutex_lock(&mutexAnalyse);
		if (nbLignesCompletes > 0) {
			for (int i=0;i<nbLignesCompletes;i++) {
				for (int j=0;j<9;j++) {
					DessineBrique(lignesCompletes[i], j, true);
				}
			}
		}
		if (nbColonnesCompletes > 0) {
			for (int i=0;i<nbColonnesCompletes;i++) {
				for (int j=0;j<9;j++) {
					DessineBrique(j, colonnesCompletes[i], true);
				}
			}
		}
		if (nbCarresComplets > 0) {
			for (int i=0;i<nbCarresComplets;i++) {
				for (int j=0;j<3;j++) {
					for (int h=0;h<3;h++) {
						DessineBrique(carresComplets[i]/3*3+j, carresComplets[i]%3*3+h, true);
					}
				}
			}
		}
		pthread_mutex_unlock(&mutexAnalyse);
	}
}
void handlerSIGUSR1(int sig) {
	CASE* empl = (CASE*)pthread_getspecific(cleCase);
	bool doublon = false;
	int a=0, b=0;
	//----------------verification des lignes--------------

	while (a<9 && tab[empl->ligne][a] == BRIQUE) a++;	//on parcour la ligne

	if (a==9) {	//si a == 9 -> la ligne est remplie
		pthread_mutex_lock(&mutexAnalyse);
		for (int i=nbLignesCompletes-1;i>=0;i--) {	//on verifie que la ligne n'a pas deja ete ajoute dans la liste des lignes completes
			if (lignesCompletes[i] == empl->ligne) doublon = true;
		}
		if (!doublon) {	//si pas encore ajoute
			lignesCompletes[nbLignesCompletes] = empl->ligne;
			nbLignesCompletes++;
		}
		pthread_mutex_unlock(&mutexAnalyse);
	}
	//----------------verification des colonnes--------------
	doublon = false;
	a=0;
	while (a<9 && tab[a][empl->colonne] == BRIQUE) a++;

	if (a==9) {
		pthread_mutex_lock(&mutexAnalyse);
		for (int i=nbColonnesCompletes-1;i>=0;i--) {
			if (colonnesCompletes[i] == empl->colonne) doublon = true;
		}
		if (!doublon) {
			colonnesCompletes[nbColonnesCompletes] = empl->colonne;
			nbColonnesCompletes++;
		}
		pthread_mutex_unlock(&mutexAnalyse);
	}
	//----------------verification des carres--------------
	doublon = false;
	a=0;
	while (a<3) {
		b=0;
		while (b<3 && tab[empl->ligne/3*3+a][empl->colonne/3*3+b] == BRIQUE) {
			b++;
		}
		if (b!=3) a=4;
		a++;
	}
	if (a == 3) {
		pthread_mutex_lock(&mutexAnalyse);
		for (int i=nbCarresComplets-1;i>=0;i--) {
			if (carresComplets[i] == empl->colonne/3 + empl->ligne/3*3) doublon = true;
		}
		if (!doublon) {
			carresComplets[nbCarresComplets] = empl->colonne/3 + empl->ligne/3*3;
			nbCarresComplets++;
		}
		pthread_mutex_unlock(&mutexAnalyse);
	}
	pthread_mutex_lock(&mutexAnalyse);
	nbAnalyses++;
	pthread_mutex_unlock(&mutexAnalyse);
	pthread_cond_signal(&condAnalyse);
}
void EndCases(void* empl) {
	printf("(CASE %u (%d,%d)) Liberation Variable specifique\n", pthread_self(), ((CASE*)empl)->ligne, ((CASE*)empl)->colonne);
	free(empl);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void threadNettoyeur() {
	struct timespec timer;
	timer.tv_sec = 2;
	int tmpcombos = 0;
	while (1) {
		pthread_mutex_lock(&mutexAnalyse);
		while (nbAnalyses < pieceEnCours.nbCases) pthread_cond_wait(&condAnalyse, &mutexAnalyse);
		if (nbLignesCompletes || nbColonnesCompletes || nbCarresComplets) {
			nanosleep(&timer, NULL);
			if (nbLignesCompletes) {
				for (int i=0;i<nbLignesCompletes;i++) {
					for (int j=0;j<9;j++) {
						tab[lignesCompletes[i]][j] = VIDE;
						EffaceCarre(lignesCompletes[i], j);
					}
					tmpcombos++;
				}
				nbLignesCompletes = 0;
			}
			if (nbColonnesCompletes) {
				for (int i=0;i<nbColonnesCompletes;i++) {
					for (int j=0;j<9;j++) {
						tab[j][colonnesCompletes[i]] = VIDE;
						EffaceCarre(j, colonnesCompletes[i]);
					}
					tmpcombos++;
				}
				nbColonnesCompletes = 0;
			}
			if (nbCarresComplets) {
				for (int i=0;i<nbCarresComplets;i++) {
					for (int j=0;j<3;j++) {
						for (int h=0;h<3;h++) {
							tab[carresComplets[i]/3*3+j][carresComplets[i]%3*3+h] = VIDE;
							EffaceCarre(carresComplets[i]/3*3+j, carresComplets[i]%3*3+h);
						}
					}
					tmpcombos++;
				}
				nbCarresComplets = 0;
			}
			pthread_mutex_lock(&mutexScore);
			score += 10*tmpcombos+5*(tmpcombos-1);
			MAJCombos = true;
			pthread_cond_signal(&condScore);
			pthread_mutex_unlock(&mutexScore);
			switch(tmpcombos) {
				case 1:
					setMessage("Simple Combo", true);
				break;
				case 2:
					setMessage("Double Combos", true);
				break;
				case 3:
					setMessage("Triple Combos", true);
				break;
				case 4:
					setMessage("Quadruple Combos", true);
				break;
				default:
					setMessage("Multiple Combos", true);
				break;
			}
			combos= combos + tmpcombos;
			tmpcombos = 0;
		}
		nbAnalyses = 0;
		pthread_mutex_unlock(&mutexAnalyse);
		pthread_mutex_lock(&mutexTraitement);
		traitementEnCours = false;
		pthread_mutex_unlock(&mutexTraitement);
		pthread_cond_signal(&condTraitement);
	}
}
