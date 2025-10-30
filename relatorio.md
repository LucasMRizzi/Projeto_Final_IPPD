# Relatório

**Primeiros passos**

Edição do código sequêncial para visualização do problema;
Identificação de funções paralelizáveis;
Verificação de condições de corrida;

**OpenMP**

### 1 - Teste de paralelização na função 'assign_points_to_clusters'

Teste realizado com paralelização básica no for triplo oculto na função.
    Comparação de resultados 3000 pontos, 5 dimensões, valores entre 0 e 1000, 10 clusters e 20 iterações:
        Sequencial -> 0.001077 ms - 24787
        OpenMP (4 threads) -> 0.000567 ms - 24787 || SpeedUp: 1.9x
        OpenMP (8 threads) -> 0.000626 ms - 24787 || SpeedUp: 1.7x
    Comparação de resultados 1000000 pontos, 10 dimensões, valores entre 0 e 10000, 100 clusters e 50 iterações:
        Sequencial -> 29.664304 ms - 5002634
        OpenMP (4 threads) -> 8.179989 ms - 5002634 || SpeedUp: 3.6x
        OpenMP (8 threads) -> 5.191413 ms - 5002634 || SpeedUp: 5.7x