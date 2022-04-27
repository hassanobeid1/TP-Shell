


// ===========================================================
// TD - intro mini shell
// SEOC 2A - 30 septembre 2020
// F. Maraninchi
// Squelette de code a utiliser pour se familiariser avec
// fork, exec, les redirections d'E/S et les wait,
// avant de se lancer dans le TP en entier.
// ATTENTION : les fichiers ne sont pas numérotés comme on
// voudrait. CORRIGEZ ça en 1er.
// ===========================================================


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>




void main() {
  pid_t pid;
  int backg;
  int fptoto;

//  /* = modèle d'utilisation de fork ====================
  printf("je suis (main) %d\n", getpid());

  switch( pid = fork() ) {

  case -1:
    perror("fork:"); break;
  case 0:
    // ici pid = 0 et ca veut dire qu'on est le fils
    printf("je suis le fils pid=%d, getpid=%d et getppid = %d\n", pid, getpid(), getppid()); break;
  default:
    // ici pid !=0, on est le père, et pid est le numéro du processus fils
    printf("je suis le père %d, mon fils est %d\n", getpid(), pid);
    break;
  }
//  ====================================================== */

  int i = 0;  // pour la numerotation du fichier toto_i.txt

  // Schema typique du shell
  // =======================
  while (1) {
    int go;
    int status;

    // ============== Lecture de la commande, a enrichir petit a petit.
    // (Essayer ensuite de coder : "ls -l | grep toto" par exemple).
    // pour commencer c'est toujours ls -l > toto_i.txt, avec i
    // qui demarre a 0 et augmente a chaque fois.

    //printf("On lit la commande. Ici ce sera  ls -l > toto_%d.txt\n", i);
    printf("On lit la commande. Ici ce sera  grep 'toto' toto_%d.txt\n", i);
    printf("stop or go (0/1)\n") ; scanf ("%d", &go);
    if (!go) exit(0);


    //execution dans un processus fils
    // ================================
    pid_t pid;
    pid = fork();
    if (pid == 0) {
      // c'est le fils, qui doit executer la commande
      // pour le moment il a le même code que le père, c'est peu utile.

      // Le fichier de sortie
      char nom[100];
      sprintf (nom, "toto_%d.txt", i);

      /* Ouverture du fichier destination en écriture (O_WRONLY)
	 si le fichier n'existe pas, on le créé (O_CREAT), s'il
	 existe, on le vide (O_TRUNC). */
      fptoto = open (nom, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
      // Preparation des parametres de execve
      // (lire la doc). Pour trouver quel programme lancer pour faire "ls",
      // tapez "which ls" sur votre machine. Ici c'est /bin/ls
      char comm[] = "/usr/bin/grep";
      char *argv[3] = { comm, "-l", NULL } ;  // tous les parametres, NULL a la fin
      char *envp[1] = { NULL };


      // MODIF E/S ici si on veut (entre fork et exec).
      // ici redirection du flot de sortie (stdout=1) vers le fichier
      // qu'on vient d'ouvrir en ecriture.
      close (1) ; // stdout
      dup2(fptoto, 1);
      close (fptoto);
      // fin MODIF

      // maintenant execve va executer la commande ls avec sortie dans le fichier
      execve(comm, argv, envp);
      // execve ne revient pas a l'appelant sauf si problème
      fprintf (stderr, "une erreur est survenue au sein de execvp\n");
    }
    else {
      i++;
      // ici on est dans le pere, cad le main du shell qui continue
      printf("commande deleguee a mon fils - attente de sa fin ici\n");
      wait(&status);
      // si on n'attend pas c'est comme si on l'avait lance en background (avec &).
    }
  }
}
