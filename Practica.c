
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h> 
#include <string.h>
#include <semaphore.h>
#include <unistd.h>

#define maxDatos 3000000


/**
* ------------PROGRAMA PRODUCTOR Y CONSUMIDORES------------
* El programa necesitara 4 valores dados en la ejecucion para poder usarse correctamente.
* Primeramente leera el fichero de entrada(introducido como primer argumento) y filtrara 
* las letras que haya dentro de el para que a continuacion,
* introduzca los numeros en un buffer circular(con un tamaño introducido como tercer argumento).
* Los consumidores (cantidad introducida como cuarto argumento) recogeran los datos del buffer 
* circular y hara distintos calculos con ellos,
* estos datos seran introducidos en un vector para que el hilo lector los coja y los escriba 
* en un fichero de salida(introducido como segundo argumento)
*----------------------------------------------------------
* */
sem_t mutexBuffer, mutexVector, hayDatoVector, hayDatoBuffer, hayHuecoBuffer;

char *ficheroEntrada;
char *ficheroSalida;
int *buffer1;
int tamBuffer;
struct recopilacion *vector;
int consumidores;

int counterVaciar = 0;  //Contador global de la posicion en el buffer
bool fin = false;       //Se usara para avisar a los hilos de consumidor que ya no hay mas datos

FILE *ficheroFinal;

struct recopilacion
{
    int numeroDatos;
	int minimo;
	int maximo;
	int suma;
	double media;
    bool terminado;
};

void *productor(void* vargp)
{
    
    char numero;
    FILE *fichero;
    fichero = fopen (ficheroEntrada, "r");   
    char caracteres[100];
    
    bool valido = true;
    int ultimoValor;        //Guardara la posicion de escritura en el buffer, para que cuando termine el for sea la ultima que se ha ocupado
    
    memset(buffer1, 0, sizeof(int)*tamBuffer);      //Establece todos los valores del buffer como 0

    for (int i = 0; feof(fichero) == 0; i++)        //Iterador del buffer
    {	
        fgets(caracteres, 100, fichero);            //Recoge la siguiente linea del fichero en la lista caracteres

        if(strlen(caracteres) == 0)     //Si strlen(caracteres) es cero se debe a que es la ultima linea del fichero
        {
            break;
        }

        for(int j = 0; j < strlen(caracteres) - 1; j++) 
        {
            
            numero = caracteres[j];
            if((numero < 47 || numero > 58)){   //No reune los requisitos para entrar en el buffer
                valido = false;
                i--;        //Resta uno al iterador del buffer para no saltarse la posicion de escritura en buffer
                break;
            }       
        }
        if (valido == true)
        {   
            sem_wait(&hayHuecoBuffer);      //Espera a que haya hueco para escribir
            buffer1[(i%tamBuffer)] = atoi(caracteres);      //Buffer circular
            sem_post(&hayDatoBuffer);       //Avisa de que hay un dato nuevo

        }
        valido = true;      //Restablece el valor
        memset(caracteres, 0, sizeof(caracteres));      //Pone a cero la lista de caracterres
        ultimoValor = i;
    }

    //Cuando acaba de escribir datos añade uno adicional que indica la finalizacion de datos en el bufefr
    sem_wait(&hayHuecoBuffer);
    buffer1[((ultimoValor + 1)%tamBuffer)] = -1;
    sem_post(&hayDatoBuffer);
    
    fclose(fichero);
    pthread_exit(NULL);
}

void *consumidor(void *arg)
{

    int* identificador = (int*) arg;    //Identificador de hilo
    struct recopilacion resultados;     //Aqui se guardaran los datos recogidos por el hilo

    int suma = 0;
    int numeroDatos = 0;
    float media;
    int minimo = 3000001;
    int maximo = 0;

    int dato;
    if (consumidores == 1)      //Para cuando solo hay un consumidor
    {
        while (buffer1[counterVaciar%tamBuffer] != -1)        //Terminara de coger datos cuando el ultimo dato sea -1
        {
            sem_wait(&hayDatoBuffer);   //Espera a que haya datos en buffer
            sem_wait(&mutexBuffer);     //Espera a entrar en seccion critica
            dato = buffer1[counterVaciar%tamBuffer];    //Recoge dato de buffer
            if(dato > maximo)
            {
	            maximo = dato;
            }
	       if(dato < minimo)
            {
	            minimo = dato; 
            }
            sem_post(&hayHuecoBuffer);  //Avisa de que ha cogido el dato y ha dejado un hueco
            sem_post(&mutexBuffer);     //Salida seccion critica
            fflush(NULL);             //Vacia memoria atascada
	        suma = suma + dato;
            counterVaciar++;
	        numeroDatos++;
              
        }
    }
    
    else                        //Para mas de un consumidor
    {      
        if (*identificador == 0)    //Primer hilo
        { 
            while (buffer1[counterVaciar%tamBuffer] != -1)        //Terminara de coger datos cuando el ultimo dato sea -1
            {
                if (fin == true)    //En el momento que un hilo ha detectado el -1 final terminan todos
                {
                    break;
                }
                fflush(NULL);            //Vacia memoria atascada
                sem_wait(&hayDatoBuffer);
                sem_wait(&mutexBuffer);
                dato = buffer1[counterVaciar%tamBuffer];
                if (dato >= 0 && dato < maxDatos/consumidores)      //Detecta si el numero esta dentro del rango del hilo
                {
                    if(dato > maximo)
                    {
                        maximo = dato;
                    }
                    if(dato < minimo)
                    {
                        minimo = dato; 
                    }
                    sem_post(&hayHuecoBuffer);
                    suma = suma + dato;
                    counterVaciar++;
                    numeroDatos++;
                }
                else                //Si no esta en el rango devulve el valor del semaforo
                {
                    sem_post(&hayDatoBuffer);
                }
                sem_post(&mutexBuffer);   
            }
            fin = true;     //Indica que ha terminado
                
        }
            
        else if(*identificador != 0 && (*identificador) != (consumidores - 1))      //Para los hilos que no son ni el primero ni el ultimo
        { 
            while (buffer1[counterVaciar%tamBuffer] != -1)        //Terminara de coger datos cuando el ultimo dato sea -1
            {
                if (fin == true)
                {
                    break;
                }
                fflush(NULL);            //Vacia memoria atascada
                sem_wait(&hayDatoBuffer);
                sem_wait(&mutexBuffer);
                dato = buffer1[counterVaciar%tamBuffer];
                    
                if (dato >= (maxDatos/consumidores)*(*identificador) && dato < (maxDatos/consumidores)*(*identificador + 1))       //Detecta si el numero esta dentro del rango del hilo
                {
                    if(dato > maximo)
                    {
                        maximo = dato;
                    }

                    if(dato < minimo)
                    {
                        minimo = dato; 
                    }
                    sem_post(&hayHuecoBuffer);
                    suma = suma + dato;
                    counterVaciar++;
                    numeroDatos++;
                }
                else
                {
                    sem_post(&hayDatoBuffer);
                }
                sem_post(&mutexBuffer);    
            }
            fin = true;      
        }
        else if(*identificador == consumidores - 1)     //Para el ultimo hilo
        {
            while (buffer1[counterVaciar%tamBuffer] != -1)        //Terminara de coger datos cuando el ultimo dato sea -1
            {
                if (fin == true)
                {
                    break;
                }  
                fflush(NULL);            //Vacia memoria atascada
                sem_wait(&hayDatoBuffer);
                sem_wait(&mutexBuffer);
                dato = buffer1[counterVaciar%tamBuffer];
                if (dato >= (maxDatos/consumidores)*(*identificador))       //Detecta si el numero esta dentro del rango del hilo
                {
                    if(dato > maximo)
                    {
                        maximo = dato;
                    }

                    if(dato < minimo)
                    {
                        minimo = dato; 
                    }
                    sem_post(&hayHuecoBuffer);
                                
                    suma = suma + dato;
                    counterVaciar++;
                    numeroDatos++;
                }
                else
                {
                    sem_post(&hayDatoBuffer);
                }
                sem_post(&mutexBuffer);
            }
            fin = true;
        }
        
    }
    
    media = (float)suma/(float)numeroDatos;

    //Guarda los datos en la estructura de recopilacion
    resultados.numeroDatos = numeroDatos;
    resultados.minimo = minimo;
    resultados.maximo = maximo;
    resultados.suma = suma;
    resultados.media = media;
    resultados.terminado = true;    //Indica que los datos estan listos para su lectura

    sem_wait(&mutexVector);
    vector[*identificador] = resultados;        //Guarda los datos en el vector
    sem_post(&hayDatoVector);
    sem_post(&mutexVector);

    pthread_exit(NULL);
}
void *lector (void* vargp)
{
    struct recopilacion dato;   //Se utilizara para crear unos datos globales
    int counter;                //Levara la cuenta de los resultados asociados a su respectivo hilo
    int restantes = consumidores;   //Ayudara para saber cuantos hilos quedan por dar el resultado
    ficheroFinal = fopen ( ficheroSalida, "w" );    //Fichero para escribir los resultados

    //Variables que guardaran unos resultados totales de todos los hilos
    int sumaTotal = 0;
    int numeroDatosTotal = 0;
    double mediaTotal = 0;
    int minimoTotal = 3000000;
    int maximoTotal = 0;

    while (restantes > 0)   
    {
        sem_wait(&hayDatoVector);
        sem_wait(&mutexVector);
        for (int i = 0; i < consumidores; i++)      //Recorre el vector en busca de informacion lista para poner en el fichero de salida
        {
            dato = vector[i];
            if (dato.terminado)             //Si el hilo ha terminado de recopilar datos
            {
                counter = i;                //Guarda el valor porque es el indentificador del hilo
                dato.terminado = false;     //Avisa de que ya ha leido los datos
                vector[i] = dato;           //Coge la informacion
                break;
            }
        }
        restantes--;        //Indica que queda un hilo menos por dar informacion
        sem_post(&mutexVector);
        //Recoge datos para los resultados globales
        sumaTotal = sumaTotal + dato.suma;
        numeroDatosTotal = numeroDatosTotal + dato.numeroDatos;
        if (minimoTotal > dato.minimo)
        {
            minimoTotal = dato.minimo;
        }
        if (maximoTotal < dato.maximo)
        {
            maximoTotal = dato.maximo;
        }

        //Escribe los datos en el fichero de salida
        fprintf(ficheroFinal,"-----Hilo numero %d-----\n", counter + 1);
        fprintf(ficheroFinal,"El numero de datos es: %d\n", dato.numeroDatos);
	    fprintf(ficheroFinal,"El minimo es: %d\n", dato.minimo);
	    fprintf(ficheroFinal,"El maximo es: %d\n", dato.maximo);
	    fprintf(ficheroFinal,"La suma total es: %d\n", dato.suma);
	    fprintf(ficheroFinal,"La media es: %f\n", dato.media);
        fprintf(ficheroFinal,"\n");   
    }
    
    //Pondra los datos globales al final
    mediaTotal = sumaTotal/numeroDatosTotal;
    fprintf(ficheroFinal,"-----Resultado total-----\n");
    fprintf(ficheroFinal,"El numero de datos es: %d\n", numeroDatosTotal);
	fprintf(ficheroFinal,"El minimo es: %d\n", minimoTotal);
	fprintf(ficheroFinal,"El maximo es: %d\n", maximoTotal);
	fprintf(ficheroFinal,"La suma total es: %d\n", sumaTotal);
	fprintf(ficheroFinal,"La media es: %f\n", mediaTotal);
    fprintf(ficheroFinal,"\n");   
    fclose (ficheroFinal);

    pthread_exit(NULL);
}

int main (int argc, char* argv[])
{
    if (argc < 5)
    {
        printf("Escribe los 4 parametros\n");
    }

    ficheroEntrada = argv[1];
    ficheroSalida = argv[2];
    tamBuffer = atoi(argv[3]);
    consumidores = atoi(argv[4]);
    int identificador[consumidores];        //Lista que guardara el numero de cada hilo
    
    buffer1 = calloc(tamBuffer, sizeof(int));
    vector = calloc(consumidores, sizeof(struct recopilacion));

    //Pone todos los valores de terminado como falso.
    struct recopilacion datos;
    datos.terminado = false;
    for(int i = 0; i < consumidores; i++)
    {
        vector[i] = datos;
    }

    //Inicializa cada semaforo
    sem_init(&mutexBuffer, 0, 1);
    sem_init(&mutexVector, 0, 1);
    sem_init(&hayDatoVector,0, 0);
    sem_init(&hayDatoBuffer,0, 0);
    sem_init(&hayHuecoBuffer,0, tamBuffer);

    pthread_t thread_productor, thread_consumidor[consumidores], thread_lector;             //Declaracion de hilo

    pthread_create(&thread_productor, NULL, productor, NULL);   //Creacion de hilo que usara la funcion productor
    for(int i = 0; i < consumidores; i++)       //Creacion de hilos consumidores
    {
        identificador[i] = i;       //Indicara el numero de hilo
        pthread_create(&thread_consumidor[i], NULL, consumidor, &identificador[i]);     //Pasa como argumento el identificador de hilo
    }
    pthread_create(&thread_lector, NULL, lector, NULL);


    pthread_join(thread_productor, NULL);   //Espera a que el hilo termine de ejecutarse
    for(int i = 0; i < consumidores; i++)
    { 
        pthread_join(thread_consumidor[i], NULL);
    }
    pthread_join(thread_lector, NULL);   //Espera a que el hilo termine de ejecutarse
    
    free (buffer1);
    free (vector);

    sem_destroy(&mutexBuffer);
    sem_destroy(&mutexVector);
    sem_destroy(&hayDatoVector);
    sem_destroy(&hayHuecoBuffer);
    sem_destroy(&hayDatoBuffer);

    return 0;
}
