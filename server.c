#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <libxml2/parser.h>
#include <libxml2/tree.h>
#include <time.h>

#define PORT 2969
#define FILE "data.xml"

int update_train_delay(char *train_id, char *delay) {
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    xmlDoc *doc = xmlReadFile(FILE, NULL, 0);
    if (doc == NULL) {perror("Error with the XML file\n");}

    xmlNode *root = xmlDocGetRootElement(doc);

    xmlNode *target_train = NULL;
    for (xmlNode *train = root->children; train; train = train->next) {
        if (train->type == XML_ELEMENT_NODE && strcmp((char *)train->name, "train") == 0) {
            xmlChar *id = xmlGetProp(train, (const xmlChar *)"id");
            if (id && strcmp((char *)id, train_id) == 0) {
                target_train = train;
                xmlFree(id);
                break;
            }
            xmlFree(id);
        }
    }

    if (target_train == NULL) {return 1;}

    xmlSetProp(target_train, (const xmlChar *)"delay", (const xmlChar *)delay);

    pthread_mutex_lock(&lock);
    if (xmlSaveFile(FILE, doc) == -1) {perror("Failed to update the XML file\n");}
    pthread_mutex_unlock(&lock);

    xmlFreeDoc(doc);
    xmlCleanupParser();

    pthread_mutex_destroy(&lock);

    return 0;
}

void convert_time(const xmlChar *time_str, int *hours, int *minutes) {
    char temp_time[10] = {0};
    strncpy(temp_time, (const char *)time_str, sizeof(temp_time) - 1);
    temp_time[sizeof(temp_time) - 1] = '\0';

    char *hour_string = strtok(temp_time, ":");
    char *minute_string = strtok(NULL, ":");

    *hours = atoi(hour_string);
    *minutes = atoi(minute_string);
}

void get_current_time(int *hour, int *minute) {
    time_t now;
    struct tm *local;
    now = time(NULL);
    local = localtime(&now);
    *hour = local->tm_hour;
    *minute = local->tm_min;
}

void reset_delays() {
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    int hour, minute;
    get_current_time(&hour, &minute);

    char *delay_reset = "0";

    xmlDoc *doc = xmlReadFile(FILE, NULL, 0);
    if (doc == NULL) {perror("Error with the XML file\n");}

    xmlNode *root = xmlDocGetRootElement(doc);
    xmlNode *destination;

    for (xmlNode *train = root->children; train; train = train->next) {
        if (train->type == XML_ELEMENT_NODE && strcmp((char *)train->name, "train") == 0) {
            xmlChar *delay_prop = xmlGetProp(train, (const xmlChar *)"delay");
            int delay = atoi((char *)delay_prop);
            xmlChar *train_name = xmlGetProp(train, (const xmlChar *)"id");

            for (xmlNode *station = train->children; station; station = station->next) {
                if (station->type == XML_ELEMENT_NODE && !strcmp((char *)station->name, "station")) {
                    xmlChar *station_name = xmlNodeGetContent(station);
                    if (station_name)
                        destination = station;
                }
            }

            xmlChar *arrival_time = xmlGetProp(destination, (const xmlChar *)"arrival");
            int arrival_hours, arrival_minutes;
            convert_time(arrival_time, &arrival_hours, &arrival_minutes);

            if (arrival_hours == 0 && hour == 23) arrival_hours = 24;

            if (arrival_hours * 60 + arrival_minutes + delay + 60 <= hour * 60 + minute) {
                xmlSetProp(train, (const xmlChar *)"delay", (const xmlChar *)delay_reset);
            }

            xmlFree(train_name);
            xmlFree(delay_prop);
            xmlFree(arrival_time);
        }
    }
    
    pthread_mutex_lock(&lock);
    if (xmlSaveFile(FILE, doc) == -1) {perror("Failed to update the XML file\n");}
    pthread_mutex_unlock(&lock);

    xmlFreeDoc(doc);
    xmlCleanupParser();

    pthread_mutex_destroy(&lock);
}

void find_trains_at_station(char *target_station, int mode, int client) {
    reset_delays();

    char response[5000] = {0};
    char train_info[50][500] = {0};
    int index = 0;

    int hour, minute;
    get_current_time(&hour, &minute);

    if (mode == 2) {
        strcpy(response, "Arrivals in the next hour to ");
        strcat(response, target_station);
        strcat(response, ":\n");
    }
    else if (mode == 3) {
        strcpy(response, "Departures in the next hour from ");
        strcat(response, target_station);
        strcat(response, ":\n");
    }
    else if (mode == 0) {
        strcpy(response, "Arrivals in the next 24 hours to ");
        strcat(response, target_station);
        strcat(response, ":\n");
    }
    else if (mode == 1) {
        strcpy(response, "Departures in the next 24 hours from ");
        strcat(response, target_station);
        strcat(response, ":\n");
    }

    xmlDoc *doc = xmlReadFile(FILE, NULL, 0);
    if (doc == NULL) {perror("Error with the XML file\n");}

    xmlNode *root = xmlDocGetRootElement(doc);
    xmlChar *destination;

    for (xmlNode *train = root->children; train; train = train->next) {
        if (train->type == XML_ELEMENT_NODE && strcmp((char *)train->name, "train") == 0) {
            xmlChar *delay_prop = xmlGetProp(train, (const xmlChar *)"delay");
            int delay = atoi((char *)delay_prop);
            xmlChar *train_name = xmlGetProp(train, (const xmlChar *)"id");

            for (xmlNode *station = train->children; station; station = station->next) {
                if (station->type == XML_ELEMENT_NODE && !strcmp((char *)station->name, "station")) {
                    xmlChar *station_name = xmlNodeGetContent(station);
                    if (station_name)
                        destination = station_name;
                }
            }

            for (xmlNode *station = train->children; station; station = station->next) {
                if (station->type == XML_ELEMENT_NODE && !strcmp((char *)station->name, "station")) {
                    xmlChar* station_name = xmlNodeGetContent(station);

                    if (strcmp((char *)station_name, target_station) == 0) {
                        xmlChar *arrival_time = xmlGetProp(station, (const xmlChar *)"arrival");
                        xmlChar *departure_time = xmlGetProp(station, (const xmlChar *)"departure");
                        if (mode == 2 && arrival_time) {
                            int arrival_hours, arrival_minutes;
                            convert_time(arrival_time, &arrival_hours, &arrival_minutes);

                            if (arrival_hours == 0 && hour == 23) arrival_hours = 24;

                            if (arrival_hours * 60 + arrival_minutes >= hour * 60 + minute && arrival_hours * 60 + arrival_minutes < hour * 60 + minute + 60) {
                                sprintf(train_info[index], "Scheduled arrival: %s, Delay: %d minute(s), Train: %s, Destination: %s\n", arrival_time, delay, train_name, destination);
                                index++;
                            }
                        }
                        else if (mode == 3 && departure_time) {
                            int departure_hours, departure_minutes;
                            convert_time(departure_time, &departure_hours, &departure_minutes);

                            if (departure_hours == 0 && hour == 23) departure_hours = 24;

                            if (departure_hours * 60 + departure_minutes >= hour * 60 + minute && departure_hours * 60 + departure_minutes < hour * 60 + minute + 60) {
                                sprintf(train_info[index], "Scheduled departure: %s, Delay: %d minute(s), Train: %s, Destination: %s\n", departure_time, delay, train_name, destination);
                                index++;
                            }
                        }
                        else if (mode == 0 && arrival_time) {
                            sprintf(train_info[index], "Scheduled arrival: %s, Delay: %d minute(s), Train: %s, Destination: %s\n", arrival_time, delay, train_name, destination);
                            index++;
                        }
                        else if (mode == 1 && departure_time) {
                            sprintf(train_info[index], "Scheduled departure: %s, Delay: %d minute(s), Train: %s, Destination: %s\n", departure_time, delay, train_name, destination);
                            index++;
                        }
                    }

                    xmlFree(station_name);
                }
            }

            xmlFree(train_name);
        }
    }

    xmlFreeDoc(doc);
    xmlFree(destination);
    xmlCleanupParser();

    char aux;
    for (int i = 0; i < index - 1; i++)
        for (int j = i + 1; j < index; j++)
            if (strcmp(train_info[i], train_info[j]) > 0)
                for (int k = 0; train_info[i][k] || train_info[j][k]; k++)
                    aux = train_info[i][k], train_info[i][k] = train_info[j][k], train_info[j][k] = aux;

    for (int i = 0; i < index; i++)
        strcat(response, train_info[i]);

    if (index == 0)
        strcat(response, "No trains found\n");

    if (write (client, response, strlen(response) * sizeof(char)) <= 0) {perror ("Write error\n");}
}

void send_data(int client, int mode) {
    char command[100] = {0}, response[1000] = {0};
    char station[100] = {0};

    strcpy(response, "Station: ");
    if (write (client, response, strlen(response) * sizeof(char)) <= 0) {perror ("Write error\n");}

    ssize_t size = read (client, station, sizeof(station) - 1);
    if (size < 0) {perror ("Read error\n");}
    else {station[size] = '\0';}

    find_trains_at_station(station, mode, client);
}

void update_data(int client) {
    char command[100] = {0}, response[1000] = {0}, train[100] = {0}, delay[100] = {0};

    strcpy(response, "Train: ");
    if (write (client, response, strlen(response) * sizeof(char)) <= 0) {perror ("Write error\n");}

    ssize_t size = read (client, train, sizeof(train) - 1);
    if (size < 0) {perror ("Read error\n");}
    else {train[size] = '\0';}

    strcpy(response, "Delay (in minutes): ");
    if (write (client, response, strlen(response) * sizeof(char)) <= 0) {perror ("Write error\n");}

    size = read (client, delay, sizeof(delay) - 1);
    if (size < 0) {perror ("Read error\n");}
    else {delay[size] = '\0';}

    int delay_int = atoi(delay) ? atoi(delay) : 0;
    char delay_numeric[100] = {0};
    sprintf(delay_numeric, "%d", delay_int);

    if (update_train_delay(train, delay_numeric) == 0) {sprintf(response, "Updated: train %s has a %s minute(s) delay\n", train, delay_numeric);}
    else {sprintf(response, "Train not found\n");}
    
    if (write (client, response, strlen(response) * sizeof(char)) <= 0) {perror ("Write error\n");}
}

void* treat(void* arg) {
    int client = *(int*)arg;
    char command[100] = {0}, response[1000] = {0};

    while (1) {
        ssize_t size = read (client, command, sizeof(command) - 1);
        if (size < 0) {perror ("Read error\n");}
        else {command[size] = '\0';}

        if (!strcmp(command, "request_daily_arrivals")) {send_data(client, 0);}
        else if (!strcmp(command, "request_daily_departures")) {send_data(client, 1);}
        else if (!strcmp(command, "request_hourly_arrivals")) {send_data(client, 2);}
        else if (!strcmp(command, "request_hourly_departures")) {send_data(client, 3);}
        else if (!strcmp(command, "update_train")) {update_data(client);}
        else if (!strcmp(command, "exit")) {
            strcpy(response, "Exiting the client application...\n");
            if (write (client, response, strlen(response) * sizeof(char)) <= 0) {perror ("Write error\n");}
            printf("Client %d has exited\n", client);
            return NULL;
        }
        else {
            strcpy(response, "Valid commands: request_daily_arrivals, request_daily_departures, request_hourly_arrivals, request_hourly_departures, update_train, exit\n");
            if (write (client, response, strlen(response) * sizeof(char)) <= 0) {perror ("Write error\n");}
            continue;
        }

        printf("Client %d sent the following command: %s\n", client, command);
    }
}

int main() {
    struct sockaddr_in server;
    struct sockaddr_in from;	
    int sd;
    pthread_t th[100];
    int i = 0;

    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1) {perror ("Socket error\n");}

    bzero (&server, sizeof (server));
    bzero (&from, sizeof (from));
  
    server.sin_family = AF_INET;	
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = htons (PORT);

    if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1) {perror ("Bind error\n");}

    if (listen (sd, 2) == -1) {perror ("Listen error\n");}

    while (1) {
        int* client = malloc(sizeof(int));
        int length = sizeof(from);

        if ((*client = accept (sd, (struct sockaddr *) &from, &length)) < 0) {perror ("Accept error\n");}

	    pthread_create(&th[i], NULL, &treat, client);

        i++;      			
	}
}