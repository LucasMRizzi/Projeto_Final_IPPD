#define _POSIX_C_SOURCE 199309L  // Necessário para CLOCK_MONOTONIC
#include <limits.h>              // Para LLONG_MAX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>  // Header correto para clock_gettime e struct timespec
#include <pthread.h>


int thread_count;

// Estrutura para representar um ponto no espaço D-dimensional
typedef struct {
  int* coords;     // Vetor de coordenadas inteiras
  int cluster_id;  // ID do cluster ao qual o ponto pertence
} Point;



typedef struct {
    Point* points;
    Point* centroids;
    int num_pontos;
    int num_clusters;
    int num_dimensoes;
    int thread_id;
    int thread_count;
} ThreadArgs;



// --- Funções Utilitárias ---

/**
 * @brief Calcula a distância Euclidiana ao quadrado entre dois pontos com coordenadas inteiras.
 * Usa 'long long' para evitar overflow no cálculo da distância e da diferença.
 * @return A distância Euclidiana ao quadrado como um long long.
 */
long long euclidean_dist_sq(Point* p1, Point* p2, int num_dimensoes) {
  long long dist = 0;
  for (int i = 0; i < num_dimensoes; i++) {
    long long diff = (long long)p1->coords[i] - p2->coords[i];
    dist += diff * diff;
  }
  return dist;
}

// --- Funções Principais do K-Means ---

/**
 * @brief Lê os dados de pontos (inteiros) de um arquivo de texto.
 */
void read_data_from_file(const char* filename, Point* points, int num_pontos, int num_dimensoes) {
  FILE* file = fopen(filename, "r");
  if (file == NULL) {
    fprintf(stderr, "Erro: Não foi possível abrir o arquivo '%s'\n", filename);
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < num_pontos; i++) {
    for (int j = 0; j < num_dimensoes; j++) {
      if (fscanf(file, "%d", &points[i].coords[j]) != 1) {
        fprintf(stderr, "Erro: Arquivo de dados mal formatado ou incompleto.\n");
        fclose(file);
        exit(EXIT_FAILURE);
      }
    }
  }

  fclose(file);
}

/**
 * @brief Inicializa os centroides escolhendo K pontos aleatórios do dataset.
 */
void initialize_centroids(Point* points, Point* centroids, int num_pontos, int num_clusters, int num_dimensoes) {
  srand(42);  // Semente fixa para reprodutibilidade

  int* indices = (int*)malloc(num_pontos * sizeof(int));
  for (int i = 0; i < num_pontos; i++) {
    indices[i] = i;
  }

  for (int i = 0; i < num_pontos; i++) {
    int j = rand() % num_pontos;
    int temp = indices[i];
    indices[i] = indices[j];
    indices[j] = temp;
  }

  for (int i = 0; i < num_clusters; i++) {
    memcpy(centroids[i].coords, points[indices[i]].coords, num_dimensoes * sizeof(int));
  }

  free(indices);
}








/**
 * @brief Fase de Atribuição: Associa cada ponto ao cluster do centroide mais próximo.
 */
void assign_points_to_clusters(Point* points, Point* centroids, int num_pontos, int num_clusters, int num_dimensoes) {
  for (int i = 0; i < num_pontos; i++) {
    long long min_dist = LLONG_MAX;
    int best_cluster = -1;

    for (int j = 0; j < num_clusters; j++) {
      long long dist = euclidean_dist_sq(&points[i], &centroids[j], num_dimensoes);
      if (dist < min_dist) {
        min_dist = dist;
        best_cluster = j;
      }
    }
    points[i].cluster_id = best_cluster;
  }
}

/**
 * Maior problema
 * for triplo -> pontos * clusters * dimensoes
 * 
 * escritas:
 *  dist -> privada
 *  min_dist -> privada
 *  best_cluster -> privada
 *  points -> pública
 */

/**
 * @brief Fase de Atualização: Recalcula a posição de cada centroide como a média
 * (usando divisão inteira) de todos os pontos atribuídos ao seu cluster.
 */
typedef struct {
    Point* points;
    int num_pontos;
    int num_clusters;
    int num_dimensoes;
    int thread_id;
    int thread_count;
    long long* local_sums;
    int* local_counts;
} ThreadArgsUpdate;

void* thread_accumulate(void* args) {
    ThreadArgsUpdate* targs = (ThreadArgsUpdate*) args;
    int start = (targs->num_pontos / targs->thread_count) * targs->thread_id;
    int end = (targs->thread_id == targs->thread_count - 1)
                ? targs->num_pontos
                : start + (targs->num_pontos / targs->thread_count);

    for (int i = start; i < end; i++) {
        int cluster_id = targs->points[i].cluster_id;
        targs->local_counts[cluster_id]++;
        for (int j = 0; j < targs->num_dimensoes; j++) {
            targs->local_sums[cluster_id * targs->num_dimensoes + j] += targs->points[i].coords[j];
        }
    }
    
    pthread_exit(NULL);
}

void update_centroids(Point* points, Point* centroids, int num_pontos, 
                      int num_clusters, int num_dimensoes, int thread_count) {
    
    long long* cluster_sums = (long long*)calloc(num_clusters * num_dimensoes, sizeof(long long));
    int* cluster_counts = (int*)calloc(num_clusters, sizeof(int));
    pthread_t* thread_handles = malloc(thread_count * sizeof(pthread_t));
    ThreadArgsUpdate* targs = malloc(thread_count * sizeof(ThreadArgsUpdate));
    
    for (int i = 0; i < thread_count; i++) {
        targs[i].local_sums = (long long*)calloc(num_clusters * num_dimensoes, sizeof(long long));
        targs[i].local_counts = (int*)calloc(num_clusters, sizeof(int));
        
        targs[i].points = points;
        targs[i].num_pontos = num_pontos;
        targs[i].num_clusters = num_clusters;
        targs[i].num_dimensoes = num_dimensoes;
        targs[i].thread_id = i;
        targs[i].thread_count = thread_count;
        
        if (pthread_create(&thread_handles[i], NULL, thread_accumulate, &targs[i]) != 0) {
            fprintf(stderr, "Erro ao criar thread %d para update_centroids\n", i);
            exit(EXIT_FAILURE);
        }
    }
  
    for (int i = 0; i < thread_count; i++) {
        pthread_join(thread_handles[i], NULL);
    }
    
    for (int t = 0; t < thread_count; t++) {
        for (int i = 0; i < num_clusters; i++) {
            cluster_counts[i] += targs[t].local_counts[i];
            for (int j = 0; j < num_dimensoes; j++) {
                cluster_sums[i * num_dimensoes + j] += targs[t].local_sums[i * num_dimensoes + j];
            }
        }

        free(targs[t].local_sums);
        free(targs[t].local_counts);
    }
    
    for (int i = 0; i < num_clusters; i++) {
        if (cluster_counts[i] > 0) {
            for (int j = 0; j < num_dimensoes; j++) {
                centroids[i].coords[j] = cluster_sums[i * num_dimensoes + j] / cluster_counts[i];
            }
        }
    }
  
    free(cluster_sums);
    free(cluster_counts);
    free(thread_handles);
    free(targs);
}



void* thread_assign_points(void* args) {
    ThreadArgs* targs = (ThreadArgs*) args;

    // Divide o trabalho entre threads
    int start = (targs->num_pontos / targs->thread_count) * targs->thread_id;
    int end = (targs->thread_id == targs->thread_count - 1)
                ? targs->num_pontos
                : start + (targs->num_pontos / targs->thread_count);

    // Chama a função original, mas só com o pedaço de pontos da thread
    assign_points_to_clusters(&targs->points[start], targs->centroids, end - start, targs->num_clusters, targs->num_dimensoes);

    pthread_exit(NULL);
}







/**
 * Ponto secundário
 * 2 fors duplos -> pontos * dimensoes, clusters * dimensoes
 * 
 * escritas:
 *  cluster_counts -> pública
 *  cluster_sums -> pública
 *  centroids -> pública
 */

/**
 * @brief Imprime os resultados finais e o checksum (como long long).
 */
void print_results(Point* centroids, int num_clusters, int num_dimensoes) {
  printf("--- Centroides Finais ---\n");
  long long checksum = 0;
  for (int i = 0; i < num_clusters; i++) {
    printf("Centroide %d: [", i);
    for (int j = 0; j < num_dimensoes; j++) {
      printf("%d", centroids[i].coords[j]);
      if (j < num_dimensoes - 1) printf(", ");
      checksum += centroids[i].coords[j];
    }
    printf("]\n");
  }
  printf("\n--- Checksum ---\n");
  printf("%lld\n", checksum);  // %lld para long long int
}

/**
 * @brief Calcula e imprime o tempo de execução e o checksum final.
 * A saída é formatada para ser facilmente lida por scripts:
 * Linha 1: Tempo de execução em segundos (double)
 * Linha 2: Checksum final (long long)
 */
void print_time_and_checksum(Point* centroids, int num_clusters, int num_dimensoes, double exec_time) {
  long long checksum = 0;
  for (int i = 0; i < num_clusters; i++) {
    for (int j = 0; j < num_dimensoes; j++) {
      checksum += centroids[i].coords[j];
    }
  }
  // Saída formatada para o avaliador
  printf("%lf\n", exec_time);
  printf("%lld\n", checksum);
}

// --- Função Principal ---

int main(int argc, char* argv[]) {

    


  // Validação e leitura dos argumentos de linha de comando
  if (argc != 7) {
    fprintf(stderr, "Uso: %s <arquivo_dados> <num_pontos> <num_dimensoes> <num_clusters> <num_iteracoes> <num_threads>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char* filename = argv[1];
  const int num_pontos = atoi(argv[2]);
  const int num_dimensoes = atoi(argv[3]);
  const int num_clusters = atoi(argv[4]);
  const int num_iteracoes = atoi(argv[5]);




    thread_count = strtol(argv[6], NULL, 10);
    if (thread_count <= 0) {
        fprintf(stderr, "Erro: número de threads inválido (%d)\n", thread_count);
        return EXIT_FAILURE;
    }
    pthread_t *thread_handles = malloc(thread_count * sizeof(pthread_t));
    ThreadArgs *targs = malloc(thread_count * sizeof(ThreadArgs));
    if (!thread_handles || !targs) {
        fprintf(stderr, "Erro: falha ao alocar memória para threads.\n");
        return EXIT_FAILURE;
    }







  if (num_pontos <= 0 || num_dimensoes <= 0 || num_clusters <= 0 || num_iteracoes <= 0 || num_clusters > num_pontos) {
    fprintf(stderr, "Erro nos parâmetros. Verifique se Numero de pontos, Numero de dimensoes, Numero de clusters,\
                        Numero de Iteracoes > 0 e Numero de clusters <= Numero de pontos.\n");
    return EXIT_FAILURE;
  }

  // --- Alocação de Memória ---
  int* all_coords = (int*)malloc((num_pontos + num_clusters) * num_dimensoes * sizeof(int));
  Point* points = (Point*)malloc(num_pontos * sizeof(Point));
  Point* centroids = (Point*)malloc(num_clusters * sizeof(Point));
  // ... (verificação de alocação) ...
  for (int i = 0; i < num_pontos; i++) {
    points[i].coords = &all_coords[i * num_dimensoes];
  }
  for (int i = 0; i < num_clusters; i++) {
    centroids[i].coords = &all_coords[(num_pontos + i) * num_dimensoes];
  }

  // --- Preparação (Fora da medição de tempo) ---
  read_data_from_file(filename, points, num_pontos, num_dimensoes);
  initialize_centroids(points, centroids, num_pontos, num_clusters, num_dimensoes);










  // --- Medição de Tempo do Algoritmo Principal ---
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);  // Inicia o cronômetro

  // Laço principal do K-Means (A única parte que será medida)
  
  /* FORK Cria as threads informadas na linha de comando */

  for (int iter = 0; iter < num_iteracoes; iter++) {
    for (int i = 0; i < thread_count; i++) {
        targs[i].points = points;
        targs[i].centroids = centroids;
        targs[i].num_pontos = num_pontos;
        targs[i].num_clusters = num_clusters;
        targs[i].num_dimensoes = num_dimensoes;
        targs[i].thread_id = i;
        targs[i].thread_count = thread_count;
        if (pthread_create(&thread_handles[i], NULL, thread_assign_points, &targs[i]) != 0) {
            fprintf(stderr, "Erro ao criar thread %d\n", i);
            exit(EXIT_FAILURE);
    
        }
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(thread_handles[i], NULL);
    }
    
    update_centroids(points, centroids, num_pontos, num_clusters, num_dimensoes, thread_count);

    
  }

    free(thread_handles);
    free(targs);

  clock_gettime(CLOCK_MONOTONIC, &end);  // Para o cronômetro










  // Calcula o tempo decorrido em segundos
  double time_taken = (end.tv_sec - start.tv_sec) + 1e-9 * (end.tv_nsec - start.tv_nsec);

  // --- Apresentação dos Resultados ---
  print_time_and_checksum(centroids, num_clusters, num_dimensoes, time_taken);

  // --- Limpeza ---
  free(all_coords);
  free(points);
  free(centroids);

  return EXIT_SUCCESS;
}