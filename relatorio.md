# Relatório

**Primeiros passos**

Edição do código sequêncial para visualização do problema;
Identificação de funções paralelizáveis;
Verificação de condições de corrida;

## Observação importante
    Cálculo do tempo das duas funções não paralelizadas ('assign_points_to_clusters' e 'update_centroids'):
        assign_points_to_clusters - 99%
        update_centroids - menor que 1%
    Conclusão: Não vale a pena paralelizar a função update_centroids

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
    
    Conclusão: Ponto importante a ser paralelizado, não foi afetado por condições de corrida como previmos

### 2 - Teste de paralelização na função 'update_centroids'

Teste realizado com paralelização básica no primeiro for duplo da função.
    Comparação de resultados 1000000 pontos, 10 dimensões, valores entre 0 e 10000, 100 clusters e 50 iterações:
        Sequencial -> 29.664304 ms - 5002634
        OpenMP (4 threads) -> 27.304870 ms - 5034443
        OpenMP (8 threads) -> 27.778409 ms - 5049768
    
        Nota: Aparentemente a função é pouco reelevante na questão de trabalho e paralelização, resultado errado -> possíve condição de corrida

        Condições de corrida: cluster_sums, cluster_counts

        Comparação de resultados depois da correção:
            Sequencial -> 29.664304 ms - 5002634
            OpenMP (4 threads) -> 28.636637 ms - 5002634
            OpenMP (8 threads) -> 27.298111 ms - 5002634

    Conclusão: A paralelização não se mostrou tão efetiva nessa parte função, apesar se ter um speedup, não foi significante, uma das causas pode ser a existência de duas condições de corrida

Teste realizado com paralelização básica no segundo for duplo da função.
    Comparação de resultados 1000000 pontos, 10 dimensões, valores entre 0 e 10000, 100 clusters e 50 iterações:
        Sequencial -> 27.645900 ms - 5002634
        OpenMP (4 threads) -> 27.350647 ms - 5002634
        OpenMP (8 threads) -> 27.180088 ms - 5002634
    
    Conclusão: Como esperado a função não é significante para paralelização

Teste realizado com a paralelização combinada nos dois fors duplos.
    Comparação de resultados 1000000 pontos, 10 dimensões, valores entre 0 e 10000, 100 clusters e 50 iterações:
        Sequencial -> 27.458599 ms - 5002634
        OpenMP (8 threads) -> 27.517199 ms - 5002634

    Conclusão: Sem resultado significativo

### 3 - Teste de paralelização nas funções 'assign_points_to_clusters' 'update_centroids' simultâneamente

Teste realizado com paralelização básica nas duas funções.
    Comparação de resultados 1000000 pontos, 10 dimensões, valores entre 0 e 10000, 100 clusters e 50 iterações:
        Sequencial -> 27.469135 ms - 5002634
        OpenMP (4 threads) -> 9.097608 ms - 5002634 || SpeedUp: 3.02x
        OpenMP (8 threads) -> 5.823352 ms - 5002634 || SpeedUp: 4.72x




**MPI**

## Tentativas de paralelização
    1 - Paralelização e transmissão do conjunto de pontos (Point *points)
        Resultados: foi possível rodar o código porém gerou resultados errados
        Conclusões: MPI_Scatter não lida bem com arrays de tipos artificiais, com isso tivemos que pensar em outra forma de transmitir os dados
    
    2 - Análise mais detalhada do problema
        Uma vez que não seria possível compartilhar o array de pontos, buscamos entender os outros dados do problema:
            Point* centroids -> um array com os centroides criados que é atualizado conforme os pontos vão sendo distribuidos
                            para um conjunto de pontos todo o conjunto de centroids é utilizado no calculo, portanto após o cálculo os centroides deveriam ser transmitidos para todos os processos
            int* all_coords -> o elemento chave para a resolução do problema, se antes não poderiamos transmitir arrays do tipo Point, com o
                            all_coords podemos, esse vetor armazena as informações de pontos e centroides, atraves de ponteiros, com isso podemos pensar na solução final
    
    3 - Paralelizando
        - Para facilitar a resolução o vetor all_coords foi dividido em 2, points_coords e cluster_coords
        
        3.1 Dividindo os dados
            Para cada processo, precisamos definir um conjunto de pontos e sua respectiva points_coords, inicialmente a tentativa foi de criar 2 scatters, um para pontos e outro para coords de pontos, essa solução não funcionou pois, como já vimos, scatter não lida bem com o tipo Point, para isso distribui-se apenas o vetor coords e depois recalcula-se os pontos locais.

            Depois foi necessário lidar com a lógica de centroides, são os mesmos para todos os processos, porém foi necessário realizar um broadcast do vetor cluster_coords para todos os processos.

        3.2 Resolvendo a lógica do loop principal
            Aqui está o maior problema para a paralelização em MPI, o loop principal é dividido em duas etapas, alocar pontos, e atualizar centroides, porém como vimos, o vetor de centroides e igual para os processos, para isso a seguinte lógica foi necessária

            assign_points_to_clusters() -> criar buffers globais para o cálculo -> modificação da função update centroids para reduzir e calcular os valores locais -> repassar os centroides atualizados -> atualizar os ponteiros -> ... (repetir)

        3.3 Recuperando os dados
            Por fim resta buscar os dados nos points_coords locais e junta-los para calcular o resultado final

## Resultados
    Comparação de resultados 1000000 pontos, 10 dimensões, valores entre 0 e 10000, 100 clusters e 50 iterações: 
        Sequencial -> 25.673548 ms - 5002634
        MPI (4 processos) -> 7.722227 ms - 5002634 | SpeedUp: 3.325x
        MPI (8 processos) -> 5.143490 ms - 5002634 | SpeedUp: 4.994x
        MPI (--use-hwthread-cpus = threads lógicas) -> 3.582064 ms - 5002634 | SpeedUp: 7.165x

    Conclusão: Resultado satisfatório na paralelização