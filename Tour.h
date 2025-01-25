

#ifndef PA3_BUNDLE_TOUR_H
#define PA3_BUNDLE_TOUR_H

#include <cstdio>
#include <pthread.h>
#include <semaphore.h>
#include <stdexcept>
#include <unistd.h>

using namespace std;

class Tour
{
private:
    int visitor_group;
    bool guide_exists;
    pthread_t guide_thread;
    bool tour_going_on;
    int visitors_inside;
    pthread_mutex_t mutex{};
    bool guide_ended_tour;

    sem_t leave_barrier{};
    sem_t enter_barrier{};
    sem_t start_tour{};

public:

    Tour(int visitors, int tour_guide) {
        if (visitors <= 0 || (tour_guide != 0 && tour_guide != 1)) {
            throw invalid_argument("An error occurred.");
        }
        this->visitor_group = visitors;
        this->guide_exists = tour_guide;
        this -> guide_thread = 0;
        this->visitors_inside = 0;
        this -> tour_going_on = false;
        this -> guide_ended_tour = false;

        pthread_mutex_init(&mutex, nullptr);
        sem_init(&enter_barrier, 0, 0);
        sem_init(&leave_barrier, 0, 0);
        sem_init(&start_tour, 0, 0);

    }

    ~Tour(){
        pthread_mutex_destroy(&mutex);
        sem_destroy(&enter_barrier);
        sem_destroy(&start_tour);
        sem_destroy(&leave_barrier);
    }

    void arrive(){

        pthread_mutex_lock(&mutex);


        printf("Thread ID: %lu | Status: Arrived at the location.\n", (unsigned long)pthread_self());

        while(tour_going_on){
            pthread_mutex_unlock(&mutex);
            sem_wait(&enter_barrier); //when tour is going on visitor wait in enter barrier, put the visitor to sleep
            pthread_mutex_lock(&mutex);
        }
        sem_post(&enter_barrier); //when tour is going on visitor wait in enter barrier, put the visitor to sleep

        visitors_inside++;

        if(guide_exists && visitors_inside - 1 == visitor_group){ //if there is a guide and excluding the guide there is number of people for a group
            tour_going_on = true;  //start the tour
            guide_thread = pthread_self(); //assign tour guide
            printf("Thread ID: %lu | Status: There are enough visitors, the tour is starting.\n", (unsigned long)pthread_self());

        }
        else if(!guide_exists && visitors_inside == visitor_group){ //if there is a guide and including the guide there is number of people for a group
            tour_going_on = true; //tour starts without a guide
            printf("Thread ID: %lu | Status: There are enough visitors, the tour is starting.\n", (unsigned long)pthread_self());
        }
        else{
            printf("Thread ID: %lu | Status: Only %d visitors inside, starting solo shots.\n", (unsigned long)pthread_self(), visitors_inside);

        }
        pthread_mutex_unlock(&mutex);
    }


    void start();

    void leave(){

        pthread_mutex_lock(&mutex);

        if(!tour_going_on){ //tour has not started
            visitors_inside--; //leave the site
            pthread_mutex_unlock(&mutex);
            printf("Thread ID: %lu | Status: My camera ran out of memory while waiting, I am leaving.\n", (unsigned long)pthread_self());
            return;
        }
        //a tour is in progress
        if(pthread_equal(pthread_self(), guide_thread)){ //there is a guide everyone except the guide will sleep until guide ends the tour
            printf("Thread ID: %lu | Status: Tour guide speaking, the tour is over.\n", (unsigned long)pthread_self()); //guide announces she is leaving, tour is over
            visitors_inside--; //guide leaves
            guide_ended_tour = true;
            for (int i = 0; i < visitor_group; i++) {
                sem_post(&leave_barrier);
            }
            pthread_mutex_unlock(&mutex);
            return;
        }

        if(guide_exists){
            while (!guide_ended_tour) {
                pthread_mutex_unlock(&mutex);  // Release lock while waiting
                sem_wait(&leave_barrier);     // Wait for the guide's signal
                pthread_mutex_lock(&mutex);   // Reacquire lock after being signaled
            }
        }
        else
        {
            sem_post(&leave_barrier);
        }

        visitors_inside--;
        printf("Thread ID: %lu | Status: I am a visitor and I am leaving.\n", (unsigned long)pthread_self());


        if (visitors_inside == 0) {
            printf("Thread ID: %lu | Status: All visitors have left, the new visitors can come.\n", (unsigned long)pthread_self());
            tour_going_on = false;  // Reset tour state
            guide_ended_tour = false;

            // Allow new visitors to enter
            for (int i = 0; i < visitor_group ; i++) {
                sem_post(&enter_barrier);
            }
        }
        pthread_mutex_unlock(&mutex);


    }
};






#endif //PA3_BUNDLE_TOUR_H
