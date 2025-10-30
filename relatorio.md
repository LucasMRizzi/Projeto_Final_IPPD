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
        SEQUENCIAL          PARALELIZADO        CORRETO         SPEEDUP
        45.190313           17.917564           Sim             2,522124
        44.216955           17.805557           Sim             2,483323
        46.007048           17.785290           Sim             2,586803