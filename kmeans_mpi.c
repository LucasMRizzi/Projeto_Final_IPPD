#define _POSIX_C_SOURCE 199309L  // Necessário para CLOCK_MONOTONIC
#include <limits.h>              // Para LLONG_MAX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>  // Header correto para clock_gettime e struct timespec
#include <mpi.h>

// Estrutura para representar um ponto no espaço D-dimensional
typedef struct {
  int* coords;     // Vetor de coordenadas inteiras
  int cluster_id;  // ID do cluster ao qual o ponto pertence
} Point;

int rank, size;

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
void update_centroids(Point* points, Point* centroids, int num_pontos, int num_clusters, int num_dimensoes, int *global_sum, int *global_count) {
  int* cluster_sums = (int*)calloc(num_clusters * num_dimensoes, sizeof(long long));
  int* cluster_counts = (int*)calloc(num_clusters, sizeof(int));

  for (int i = 0; i < num_pontos; i++) {
    int cluster_id = points[i].cluster_id;
    cluster_counts[cluster_id]++;
    for (int j = 0; j < num_dimensoes; j++) {
      cluster_sums[cluster_id * num_dimensoes + j] += points[i].coords[j];
    }
  }

  MPI_Allreduce(cluster_sums, global_sum,
                  num_clusters * num_dimensoes,
                  MPI_INT, MPI_SUM, MPI_COMM_WORLD);

  MPI_Allreduce(cluster_counts, global_count,
                num_clusters,
                MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  

  if(rank == 0) {
    for (int i = 0; i < num_clusters; i++) {
      if (global_count[i] > 0) {
        for (int j = 0; j < num_dimensoes; j++) {
          // Divisão inteira para manter os centroides em coordenadas discretas
          centroids[i].coords[j] = global_sum[i * num_dimensoes + j] / global_count[i];
        }
      }
    }
  }
  
  free(cluster_sums);
  free(cluster_counts);
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

  MPI_Init(&argc, &argv); // Ambiente MPI

  double start, stop;

  // Validação e leitura dos argumentos de linha de comando
  if (argc != 6) {
    fprintf(stderr, "Uso: %s <arquivo_dados> <num_pontos> <num_dimensoes> <num_clusters> <num_iteracoes>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char* filename = argv[1];
  const int num_pontos = atoi(argv[2]);
  const int num_dimensoes = atoi(argv[3]);
  const int num_clusters = atoi(argv[4]);
  const int num_iteracoes = atoi(argv[5]);

  if (num_pontos <= 0 || num_dimensoes <= 0 || num_clusters <= 0 || num_iteracoes <= 0 || num_clusters > num_pontos) {
    fprintf(stderr, "Erro nos parâmetros. Verifique se Numero de pontos, Numero de dimensoes, Numero de clusters,\
                        Numero de Iteracoes > 0 e Numero de clusters <= Numero de pontos.\n");
    return EXIT_FAILURE;
  }

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  int* points_coords;
  int* cluster_coords;
  Point* points;
  Point* centroids;

  centroids = (Point*)malloc(num_clusters * sizeof(Point));
  cluster_coords = (int *)malloc(num_clusters * num_dimensoes * sizeof(int));
  for (int i = 0; i < num_clusters; i++) {
    centroids[i].coords = &cluster_coords[i * num_dimensoes];
  }

  if (rank == 0) {
    points_coords = (int*)malloc(num_pontos * num_dimensoes * sizeof(int));
    points = (Point*)malloc(num_pontos * sizeof(Point));
    for (int i = 0; i < num_pontos; i++) {
      points[i].coords = &points_coords[i * num_dimensoes];
    }

    

    read_data_from_file(filename, points, num_pontos, num_dimensoes);
    initialize_centroids(points, centroids, num_pontos, num_clusters, num_dimensoes);
  } 


  int *send_counts_coords = malloc(sizeof(int) * size);
  int *send_place_coords = malloc(sizeof(int) * size);
  
  int base = num_pontos / size;
  int resto = num_pontos % size;

  int local_num_points = base + (rank < resto ? 1 : 0);
  int* local_points_coords = (int*)malloc(local_num_points * num_dimensoes * sizeof(int));
  Point* local_points = (Point*)malloc(local_num_points * sizeof(Point));


  if (rank == 0) {
      int offset = 0;
      for (int i = 0; i < size; i++) {
          int n_points = base + (i < resto ? 1 : 0);
          send_counts_coords[i] = n_points * num_dimensoes;
          send_place_coords[i] = offset;
          offset += send_counts_coords[i];
      }
  }
  
  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Scatterv(points_coords, send_counts_coords, send_place_coords, MPI_INT,
                local_points_coords, local_num_points * num_dimensoes, MPI_INT,
                0, MPI_COMM_WORLD);
  MPI_Bcast(cluster_coords,
          num_clusters * num_dimensoes,
          MPI_INT,
          0,
          MPI_COMM_WORLD);
  

  for (int i = 0; i < local_num_points; i ++){
    local_points[i].coords = &local_points_coords[i * num_dimensoes];
  }

  MPI_Barrier(MPI_COMM_WORLD);
  start = MPI_Wtime();

  for (int iter = 0; iter < num_iteracoes; iter++) {
    assign_points_to_clusters(local_points, centroids, 
                              local_num_points, num_clusters, num_dimensoes);

    int *global_sum = calloc(num_clusters * num_dimensoes, sizeof(int));
    int *global_count = calloc(num_clusters, sizeof(int));

    update_centroids(local_points, centroids, local_num_points, num_clusters, num_dimensoes, global_sum, global_count);

    MPI_Bcast(cluster_coords,
              num_clusters * num_dimensoes,
              MPI_INT,
              0, MPI_COMM_WORLD);

    for (int c = 0; c < num_clusters; c++) {
        centroids[c].coords = &cluster_coords[c * num_dimensoes];
    }
    
    free(global_sum);
    free(global_count);
}

  MPI_Barrier(MPI_COMM_WORLD);
  stop = MPI_Wtime();

  MPI_Gatherv(local_points_coords, local_num_points * num_dimensoes, MPI_INT,
            points_coords, send_counts_coords, send_place_coords, MPI_INT,
            0, MPI_COMM_WORLD);
  
  double time_taken = stop - start;

  // --- Apresentação dos Resultados ---
  if(rank == 0){
    print_time_and_checksum(centroids, num_clusters, num_dimensoes, time_taken);
  }

  // --- Limpeza ---
  free(local_points_coords);
  free(local_points);
  free(cluster_coords);
  free(send_counts_coords);
  free(send_place_coords);
  free(centroids);
  if (rank == 0) {
    free(points);
    free(points_coords);
  }

  MPI_Finalize();

  return EXIT_SUCCESS;
}