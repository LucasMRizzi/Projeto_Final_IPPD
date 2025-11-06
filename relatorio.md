# Relatório

**Primeiros passos**

Edição do código sequêncial para visualização do problema;
Identificação de funções paralelizáveis;
Verificação de condições de corrida;

**Pthreads**

***Paralelização na função assign_points_to_clusters***
    1 - Foi criado um struct para viabilizar a passagem de parâmetros, já que pthreads aceita apenas um formato de função para ser paralelizada (void* func(void* arg)). Struct contém os elementos:
    Point* points;
    Point* centroids;
    int num_pontos;
    int num_clusters;
    int num_dimensoes;
    int thread_id;
    int thread_count; e recebe o nome de ThreadArgs.
    Além disso foi feita uma função de passagem entre o fork e a função desejada (assign_points_to_clusters), sendo a função void* thread_assign_points(void* args), que divide o trabalho e chama a função principal.
    2 - Testes: Comparação entre sequencial e paralelizado
    Dia 1:
        SEQUENCIAL          PARALELIZADO        CORRETO         SPEEDUP
        45.190313           17.917564           Sim             2,522124
        44.216955           17.805557           Sim             2,483323
        46.007048           17.785290           Sim             2,586803
    
    Dia 2:
        SEQUENCIAL          PARALELIZADO        CORRETO         SPEEDUP
        24.981434           9.144942            Sim             2,731721
        24.104122           12.548625           Sim             1,920857
        23.930558           9.109715            Sim             2,626927

***Paralelização na função update_centroids***
    1 - Foi criado um struct para viabilizar a passagem de parâmetros para a paralelização da função update_centroids, seguindo o padrão de pthreads (void* func(void* arg)). O struct contém os elementos:
    Point* points;
    int num_pontos;
    int num_clusters;
    int num_dimensoes;
    int thread_id;
    int thread_count;
    long long* local_sums;
    int* local_counts; e recebe o nome de ThreadArgsUpdate.
    Além disso foi implementada a função void* thread_accumulate(void* args) que divide o trabalho entre as threads, onde cada thread processa um subconjunto dos pontos e acumula resultados em arrays locais privados (local_sums e local_counts), evitando as condições de corrida. A paralelização foi aplicada apenas no primeiro loop, enquanto o segundo loop de cálculo das médias permanece sequencial por ter a complexidade bem menor.

    2 - Testes: Comparação entre sequencial e paralelizado (realizado com dataset.txt, 1000000, 10, 100, 50, 8)

        SEQUENCIAL          PARALELIZADO       CORRETO         SPEEDUP
        31.940996           6.993517           Sim             4,567229
        31.975969           7.058396           Sim             4,530203
        31.957209           7.040715           Sim             4,538915