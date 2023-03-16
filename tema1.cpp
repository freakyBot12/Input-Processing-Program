#include <pthread.h>
#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <string>
#include <vector>
#include <numeric>
#include <math.h>
#include <algorithm>

// Structura de date folosita fisierele care vor fi parsate
// de thread-urile Mappere
struct map_files_t {
    std::vector<std::vector<std::vector<long>>> *generated_lists;
    std::vector<std::string> files;
    pthread_mutex_t *mutex;
    pthread_barrier_t *barrier;
    long *current_iteration;
    long exponent;
    long thread_id;

};

// Structura de date folosita pentru concatenarea listelor 
// (generate de Mappere) de catre thread-urile Reducer
struct reduce_lists_t {
    std::vector<std::vector<std::vector<long>>> *generated_lists;
    long thread_id;
    long number_of_mappers;
    pthread_barrier_t *barrier;
};

// Functia executata de fiecare thread de tip Mapper
void *mapping_function(void *arg) {
    std::string file;
    map_files_t &files_to_filter = *(map_files_t*)arg;

    while (true) {

        // Un singur thread Mapper va putea alege, la un moment dat,
        // un fisier din lista de fisiere
        pthread_mutex_lock((files_to_filter.mutex));

        // Daca nu mai exista fisiere de parsat, executia thread-ului sare la
        // finalul functiei
        if (*files_to_filter.current_iteration == (long)files_to_filter.files.size()) {
            pthread_mutex_unlock((files_to_filter.mutex));
            break;
        }

        // Fisierele vor fi selectate pentru parsare de catre thread-uri in ordinea
        // din fisierul de fisiere
        file = files_to_filter.files[*files_to_filter.current_iteration];
        (*files_to_filter.current_iteration)++;
        pthread_mutex_unlock((files_to_filter.mutex));

        long number;
        std::ifstream chosen_file(file);

        // Vom stoca numarul de numere din fisierul ales de thread (chosen file)
        // in variabila "number_of_numbers"
        int number_of_numbers;
        chosen_file >> number_of_numbers;

        // "number" va reprezenta fiecare numar din lista de numere
        while(chosen_file >> number) {
            if (number <= 0) {
                continue;
            }

            // Pentru fiecare putere de la 2 la (numarul de thread-uri Reducer + 1)
            // se va verifica, folosind Binary Search, daca "number" este putere
            // perfecta
            for (long i = 2; i <= files_to_filter.exponent + 1; i++) {
                long low = 1;
                long high = number;

                while (low <= high) {
                    long mid = (low + high) / 2;
                    double perfect_power = pow(mid, i);
                    if (perfect_power == number) {

                        // daca "number" este o putere perfecta, acesta se va stoca in matricea
                        // de vectori pe linia corespunzatoarea Mapperului care a determinat
                        // ca "number" este putere perfecta si pe coloana corespunzatoare
                        // puterii lui "number" (ex.: "number" este puterea perfecta a lui 2 =>
                        // "number" va fi adaugat in vectorul de pe coloana corespunzatoare
                        // puterii 2, care este coloana 0)
                        (*files_to_filter.generated_lists)[files_to_filter.thread_id][i - 2].push_back(number);
                        break;

                    } else if (perfect_power > number) {
                        high = mid - 1;
                    } else {
                        low = mid + 1;
                    }
                }
            }
        }


    }

    // Executia thread-urilor Reducer va incepe dupa ce toate
    // fisierele au fost parsate de thread-urile Mapper
    pthread_barrier_wait(files_to_filter.barrier);
    pthread_exit(NULL);

}

// Functia executata de fiecare thread de tip Reducer 
void *reduce_function(void *arg) {
    reduce_lists_t &files_to_reduce = *(reduce_lists_t*)arg;

    // Executia thread-urilor Reducer va incepe dupa ce thread-urile Mapper
    // au parsat toate fisierele din lista de fisiere
    pthread_barrier_wait(files_to_reduce.barrier);

    std::vector<long> mergedLists;

    // Se vor combina vectorii de pe coloana corespunzatoare puterii de care
    // raspunde thread-ul Reducer (fiecare vector corespunde unui thread Mapper)
    for (int i = 0; i < files_to_reduce.number_of_mappers; i++) {
        std::vector<long> &list = (*files_to_reduce.generated_lists)[i][files_to_reduce.thread_id];
        for (auto const &elem: list) {

            // Combinarea vectorilor se va face eliminand elementele duplicate
            if (std::find(mergedLists.begin(), mergedLists.end(), elem) == mergedLists.end()) {
                mergedLists.push_back(elem);
            }
        }
    }
    std::string output_file = "out" + std::to_string(files_to_reduce.thread_id + 2) + ".txt";
    std::ofstream final_result(output_file);
    final_result << (long)mergedLists.size();

    pthread_exit(NULL);
}


int main(int argc, char *argv[]) {
    long number_of_mappers = atol(argv[1]);
    long number_of_reducers = atol(argv[2]);
    int verify_thread;

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    // Intrucat thread-urile Reducer ajung mai repede la bariera decat thread-urile
    // Mapper, iar thread-urile Mapper trebuie sa parseze toate fisierele inainte
    // ca thread-urile Reducer sa-si inceapa executia, este necesar ca toate
    // thread-urile (atat Reducer, cat si Mapper) sa ajunga la bariera inainte de
    // inceperea executiei thread-urilor Reducer
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, number_of_mappers + number_of_reducers);
    
    std::ifstream list_of_files(argv[3]);
    long number_of_files;
    std::string file;
    list_of_files >> number_of_files;
    std::vector<std::string> files;

    // Stocam fisierele din lista de fisiere intr-un vector.
    // Accesarea acestui vector de catre Mappere va reprezenta
    // zona critica
    for (int i = 0; i < number_of_files; i++) {
        list_of_files >> file;
        files.push_back(file);
    }
    list_of_files.close();

    // Datele cu care vor lucra Mapperele
    map_files_t files_to_filter;

    // Mutex pentru accesarea zonei critice de catre Mappere
    files_to_filter.mutex = &mutex;

    // Bariera este comuna pentru Mappere si Reducere
    files_to_filter.barrier = &barrier;

    // Fisierele parsate de Mappere
    files_to_filter.files = files;

    // Se va retine parcurgerea listei de fisiere folosind variabila
    // "iteration" pentru a preveni parsarea unui fisier de mai multe Mappere
    long iteration = 0;
    files_to_filter.current_iteration = &iteration;

    // Puterea maxima pana la care Mapperul va verifica daca un numar este
    // putere perfecta
    files_to_filter.exponent = number_of_reducers;

    // Matrice de vectori in care vor fi retinute listele partiale,
    // generate de Mappere
    std::vector<std::vector<std::vector<long>>> generated_lists;
    
    for (long i = 0; i < number_of_mappers; i++) {
        // Fiecare linie a matricei contine listele partiale generate de un
        // Mapper
        std::vector<std::vector<long>> generated_lists_by_one_mapper;

        for (long j = 0; j < number_of_reducers; j++) {

            // "one_list" va contine numerele care sunt puteri ale unui
            // anumit exponent (2 pe coloana 0, 3 pe coloana 1, etc.)
            std::vector<long> one_list;
            generated_lists_by_one_mapper.push_back(one_list);
        }

        generated_lists.push_back(generated_lists_by_one_mapper);
    }
    files_to_filter.generated_lists = &generated_lists;

    // Mapperele trebuie sa prelucreze aceeasi matrice de vectori, sa
    // incrementeze acelasi "current_iteration",sa foloseasca acelasi
    // mutex si aceeasi bariera pentru id-uri diferite ale Mapperelor
    std::vector<map_files_t> vec_of_files;
    for (long i = 0; i < number_of_mappers; i++) {
        files_to_filter.thread_id = i;
        vec_of_files.push_back(files_to_filter);
    }

    // Datele cu care vor lucra Reducerele
    reduce_lists_t lists_to_reduce;

    // Matricea de vectori va fi comuna cu cea populata de
    // Mappere
    lists_to_reduce.generated_lists = &generated_lists;
    lists_to_reduce.barrier = &barrier;
    lists_to_reduce.number_of_mappers = number_of_mappers;

    // Reducerele trebuie sa lucreze cu aceeasi matrice de vectori
    // si aceeasi bariera pentru id-uri diferite ale Reducerelor
    std::vector<reduce_lists_t> vec_of_lists_to_reduce;
    for (long i = 0; i < number_of_reducers; i++) {
        lists_to_reduce.thread_id = i;
        vec_of_lists_to_reduce.push_back(lists_to_reduce);
    }

    pthread_t mappers[number_of_mappers];
    pthread_t reducers[number_of_reducers];

    for (long i = 0; i < number_of_mappers; i++) {
        verify_thread = pthread_create(&mappers[i], NULL, mapping_function, &vec_of_files[i]);
        if (verify_thread) {
            std::cout<< "Eroare la crearea thread-ului " << i << std::endl;
            exit(-1);
        }
    }
    for (long i = 0; i < number_of_reducers; i++) {
        verify_thread = pthread_create(&reducers[i], NULL, reduce_function, &vec_of_lists_to_reduce[i]);
        if (verify_thread) {
            std::cout<< "Eroare la crearea thread-ului " << i << std::endl;
            exit(-1);
        }
    }

    for (long i = 0; i < number_of_mappers; i++) {
        verify_thread = pthread_join(mappers[i], NULL);
        if (verify_thread) {
            std::cout <<"Eroare la asteptarea thread-ului " << i <<std::endl;
        }
    }

    for (long i = 0; i < number_of_reducers; i++) {
        verify_thread = pthread_join(reducers[i], NULL);
        if (verify_thread) {
            std::cout <<"Eroare la asteptarea thread-ului " << i <<std::endl;
        }
    }

    pthread_mutex_destroy(&mutex);
    pthread_barrier_destroy(&barrier);

    return 0;
}