#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <math.h>
	
#define TRUE 1
#define FALSE 0
#define PORT 8888
#define BUF_SIZE 10000//2048
#define POP 30
#define MESSAGE_LENGTH 60
#define RADIUS_EARTH 6371  // Earth radius in kilometers

enum operations {echo, measure, aggregate, dtosm, smtod, smwait, endagg};

struct latlong{
	int degree;
	int minute;
	int second;
};

struct SM
{
    struct latlong latitude;
    struct latlong longitude;
    char* id;
};


double toRadians(double degree) {
    return degree * (M_PI / 180.0);
}

double dmsToDecimal(int degrees, int minutes, int seconds) {
    return degrees + ((float) minutes / 60.0) + ((float) seconds / 3600.0);
}

double haversine(struct latlong _lat1, struct latlong _lon1, struct latlong _lat2, struct latlong _lon2) {
    // Convert latlong structures to doubles
    double lat1 = dmsToDecimal(_lat1.degree, _lat1.minute, _lat1.second);
    double lon1 = dmsToDecimal(_lon1.degree, _lon1.minute, _lon1.second);
    double lat2 = dmsToDecimal(_lat2.degree, _lat2.minute, _lat2.second);
    double lon2 = dmsToDecimal(_lon2.degree, _lon2.minute, _lon2.second);
    
    // Convert latitude and longitude from degrees to radians
    lat1 = toRadians(lat1);
    lon1 = toRadians(lon1);
    lat2 = toRadians(lat2);
    lon2 = toRadians(lon2);

    // Haversine formula
    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;
    double a = sin(dlat / 2) * sin(dlat / 2) + cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    // Distance in kilometers
    double distance = RADIUS_EARTH * c;
    return distance;
}

void print_latlong(struct latlong x){
    printf("Coord: %dº%d'%d\"\n", x.degree, x.minute, x.second);
    return;
}

void print_SM(struct SM sm)
{
    printf("SM %s:\n", sm.id);
    printf("Latitude: ");
    print_latlong(sm.latitude);
    printf("Longitude: ");
    print_latlong(sm.longitude);
    return;
}

int randrange(int lower, int upper){
    return (rand() % (upper - lower + 1)) + lower;
}

struct latlong convertDistanceToLatLong(double distanceInKm) {
    double degreeOfLatitude = 111.0;
    double degrees = distanceInKm / degreeOfLatitude;
    int intDegrees = (int)degrees;
    double remainingMinutes = (degrees - intDegrees) * 60;
    int intMinutes = (int)remainingMinutes;
    double remainingSeconds = (remainingMinutes - intMinutes) * 60;
    int intSeconds = (int) remainingSeconds;
    struct latlong ret;
    ret.degree = intDegrees;
    ret.minute = intMinutes;
    ret.second = intSeconds;
    return ret;
}

struct SM generate_SM(struct latlong min_lat, struct latlong max_lat, struct latlong min_long, struct latlong max_long){
    struct SM newSM;
    newSM.id = "new";
    float rand_0_1 = (float)rand()/(float)RAND_MAX;
    newSM.latitude.degree = randrange(min_lat.degree, max_lat.degree);
    newSM.latitude.minute = randrange(min_lat.minute, max_lat.minute);
    newSM.latitude.second = randrange(min_lat.second, max_lat.second);
    newSM.longitude.degree = randrange(min_long.degree, max_long.degree);
    newSM.longitude.minute = randrange(min_long.minute, max_long.minute);
    newSM.longitude.second = randrange(min_long.second, max_long.second);
    return newSM;
}

void generate_neighbourhood(struct SM* smart_meter_locations){
	int neighbourhood_size = 10; //square kilometers

	struct latlong starting_latitude;
	starting_latitude.degree = 44;
	starting_latitude.minute = 14;
    starting_latitude.second = 25;

    struct latlong starting_longitude;
    starting_longitude.degree = 76;
    starting_longitude.minute = 31;
    starting_longitude.second = 7;

	srand((unsigned) getpid()%10 * 100000);
    float rand_0_1 = (float)rand()/(float)RAND_MAX;
	float width = rand_0_1 * neighbourhood_size;
    float height = neighbourhood_size / width;
    printf("%f,%f\n", width, height);

    struct latlong lat_delta = convertDistanceToLatLong((double) height);
	struct latlong max_latitude;
	max_latitude.degree = starting_latitude.degree + lat_delta.degree;
    max_latitude.minute = starting_latitude.minute + lat_delta.minute;
    max_latitude.second = starting_latitude.second + lat_delta.second;

    struct latlong long_delta = convertDistanceToLatLong((double) width);
    struct latlong max_longitude;
    max_longitude.degree = starting_longitude.degree + long_delta.degree;
    max_longitude.minute = starting_longitude.minute + long_delta.minute;
    max_longitude.second = starting_longitude.second + long_delta.second;

    print_latlong(max_latitude);
    print_latlong(max_longitude);

    printf("Distance from min to max: %f\n", haversine(starting_latitude, starting_longitude, max_latitude, max_longitude));

	for(int i=0; i<POP; i++){
		smart_meter_locations[i] = generate_SM(starting_latitude, max_latitude, starting_longitude, max_longitude);
	}
}

void swap(int* arr, int a, int b){
    int temp = arr[a];
    arr[a] = arr[b];
    arr[b] = temp;
}

void generate_groups(int _population, int _groupsize, int *next_sms, int *leadership)
{
	int population = _population;
    int groupsize = _groupsize;
    int numgroups = 0;
    int remainder = 0;

    int *meters;
    int *groups;
    int *assignments;
    //int *nexts;
    int *leaders;

    numgroups = population / groupsize;
    remainder = population - groupsize * numgroups;

    meters = (int*) malloc(population * sizeof(int));
    assignments = (int*) malloc(population * sizeof(int));
    groups = (int*) malloc(numgroups * sizeof(int));
    //nexts = (int*) malloc(population * sizeof(int));
    leaders = (int*) malloc(numgroups * sizeof(int));
    
    /* reset group assignments */
    memset(assignments, 0, population);

    srand(time(NULL));

    /* reset meter positions */
    for(int i=0; i<population; i++)
    {
        meters[i] = i;
    }

    int meters_remaining = population;

    /* generate groups by randomly assigning each group to a meter
    (effectively assigning that meter to a group) */
    for(int i=0; i < numgroups; i++)
    {
        int leader = -1;
        int prev = -1;

        int current_groupsize = ((i < numgroups - 1) ? groupsize : (remainder + groupsize));
        for(int j=0; j < current_groupsize; j++)
        {
            int num = meters_remaining ? rand() % meters_remaining : 0;
            meters_remaining--;
            assignments[meters[num]] = i;
			leadership[meters[num]] = 0;

            /* create the plan */
            if(j == 0)
            {
                leader = meters[num];
				leadership[meters[num]] = 1;
                prev = leader;
                leaders[i] = leader;
            }
            else if(j == current_groupsize - 1)
            {
                next_sms[prev] = meters[num];
                next_sms[meters[num]] = leader;
                prev = meters[num];
            }
            else
            {
                next_sms[prev] = meters[num];
                prev = meters[num];
            }

            /* remove the selected meter from the pool */
            swap(meters, num, meters_remaining);
        }
    }
	    /* verification */
    for(int i=0; i<population; i++)
    {
        groups[assignments[i]]++;
    }
    for(int i=0; i<numgroups; i++)
    {
        printf("Group %d: %d\n", i, groups[i]);
    }
    for(int i=0; i<population; i++)
    {
        printf("%d: %d\n", i, next_sms[i]);
    }
    for(int i=0; i<numgroups; i++)
    {
        printf("Group %d:\n", i);
        int next = next_sms[leaders[i]];
        printf("%d->", leaders[i]);
        while(next != leaders[i])
        //for(int j=0; j < ((i < numgroups - 1) ? groupsize : (remainder + groupsize)); j++)
        {
            printf("%d->", next);
            next = next_sms[next];
        }
        printf("%d\n", next);
    }
}
	
int main(int argc , char *argv[])
{
	int opt = TRUE;
	int master_socket, addrlen, new_socket, client_socket[POP],
		max_clients = POP, activity, i, valread, sd, num_sms = 0;
	int max_sd;
	struct sockaddr_in address;
	int stage = -2;

	struct SM smart_meter_locations[POP];

	int randleader_test = 0;
	int has_generated_leader = 0;
	int has_generated_next[POP];
	int leadership[POP];
	int next_sms[POP];
	int has_generated_groups = 0;
	int results_remaining = 0;
		
	char in_buffer[BUF_SIZE], out_buffer[BUF_SIZE], 
	cmd_buf[BUF_SIZE], enc_data_buf[POP][BUF_SIZE], pk_buf[POP][BUF_SIZE];
    char data_buffer[POP][10000];
	char out_data_buffer[10000];
	char dso_data_buffer[POP][10000];
	char data_buf[POP][BUF_SIZE];
	bzero(in_buffer, BUF_SIZE);
	bzero(out_buffer, BUF_SIZE);
	bzero(cmd_buf, BUF_SIZE);
	bzero(enc_data_buf, BUF_SIZE);
	for(int i=0; i<POP; i++){
		printf("%d", leadership[i]);
	}
	printf("\n");

	generate_neighbourhood(smart_meter_locations);

	int n_groups = 0;
	int group_results[POP];

	int current_round[POP];

	int current_round_pop = 0;


	srand(time(NULL) + getpid());
		
	//set of socket descriptors
	fd_set readfds;
		
	//a message
	char *message = "DSO-\n";

	int num_sms_a;

	int sleep_incr = 0;

	printf("Enter smart meter population:");
	scanf("%d", &num_sms_a);
	num_sms = num_sms_a;
	
	struct timespec start, end;

	//initialise all client_socket[] to 0 so not checked
	for(i = 0; i < max_clients; i++)
	{
		client_socket[i] = 0;
	}
		
	//create a master socket
	if((master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
	
	//set master socket to allow multiple connections ,
	//this is just a good habit, it will work without this
	if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
		sizeof(opt)) < 0 )
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	
	//type of socket created
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );
		
	//bind the socket to localhost port 8888
	if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	printf("Listener on port %d \n", PORT);
		
	//try to specify maximum of 3 pending connections for the master socket
	if (listen(master_socket, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}
		
	//accept the incoming connection
	addrlen = sizeof(address);
	puts("Waiting for connections ...");
		
	
	/* MAIN LOOP */
	while(TRUE)
	{

		if(!strncmp(cmd_buf, "request", 7) && !has_generated_groups)
		{
			clock_gettime(CLOCK_MONOTONIC_RAW, &start);
			current_round_pop = 0;
			for (i = 0; i < max_clients; i++)
			{
				for (int u = 0; u < 2; u++){
					sd = client_socket[i];
					if(FD_ISSET(sd, &readfds))
					{
						if((valread = read(sd, in_buffer, BUF_SIZE)) == 0)
						{
							getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
							printf("Host disconnected, ip %s, port %d.\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
							close(sd);
							client_socket[i] = 0;
						}
						else
						{
							if(u == 1)
								current_round_pop += 1;

							/* transmit buffer contents */
							sprintf(out_buffer, "5#ready, #%d.", i);
							printf("Sending to #%d: %s\n", i, out_buffer);
							send(sd, out_buffer, strlen(out_buffer), 0);
							bzero(out_buffer, BUF_SIZE);		
						}
					}
				}
			}
			generate_groups(current_round_pop, 4, next_sms, leadership);
			has_generated_groups = 1;
			//printf("GGEN\n");
		}

		//clear the socket set
		FD_ZERO(&readfds);
	
		//add master socket to set
		FD_SET(master_socket, &readfds);
		max_sd = master_socket;
			
		//add child sockets to set
		for ( i = 0 ; i < max_clients ; i++)
		{
			//socket descriptor
			sd = client_socket[i];
				
			//if valid socket descriptor then add to read list
			if(sd > 0)
				FD_SET( sd , &readfds);
				
			//highest file descriptor number
			if(sd > max_sd)
				max_sd = sd;
		}
	
		//wait for activity on one of the sockets, timeout is NULL
		//so wait indefinitely
		activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);
	
		if ((activity < 0) && (errno!=EINTR))
		{
			printf("select error");
		}
			
		//incoming connection
		if (FD_ISSET(master_socket, &readfds))
		{
			if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
			{
				perror("accept");
				exit(EXIT_FAILURE);
			}
			
			//inform user of socket number - used in send and receive commands
			printf("New connection, socket fd: %d, ip: %s, port: %d\n", 
			new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

			sprintf(out_buffer, "Connected to port %d", ntohs(address.sin_port));
		
			//send new connection greeting message
			if(send(new_socket, out_buffer, strlen(out_buffer), 0) != strlen(out_buffer))
			{
				perror("send");
			}
				
			puts("Initialization successful.");
				
			//add new socket to array of sockets
			for(i = 0; i < max_clients; i++)
			{
				//if position is empty
				if(client_socket[i] == 0)
				{
					client_socket[i] = new_socket;
					printf("Adding to list of sockets as %d\n" , i);
						
					break;
				}
			}

			num_sms -= 1;
		}

		for (i = 0; i < max_clients; i++)
		{
			sd = client_socket[i];
			if(FD_ISSET(sd, &readfds))
			{
				if((valread = read(sd, in_buffer, BUF_SIZE)) == 0)
				{
					getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
					printf("Host disconnected, ip %s, port %d.\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
					close(sd);
					client_socket[i] = 0;
				}

				else
				{
					enum operations operation = -1;
					int finished_sending = 0;
    				int msg_position = 0;

					/* handle message */
					while(operation != smwait)
					{

						/* print the incoming message */
						printf("SM#%d: %s\n", i, in_buffer);

						operation = atoi(strtok(in_buffer, "#"));

						sprintf(out_buffer, "5#Ready, #%d.", i);

						if(operation == smtod)
						{
							//printf("\nSMTOD:\n");
							int end_of_message = atoi(strtok(NULL, "#"));
							strcat(data_buffer[next_sms[i]], strtok(NULL, "#"));
							//printf("%s\n", data_buffer[next_sms[i]]);
							if(!end_of_message)
							{
								sprintf(out_buffer, "4#");
							}
						}

						if(!strncmp(data_buffer[i], "2", 1))
						{
							operation = dtosm;
                            sprintf(out_data_buffer, "%s", data_buffer[i] + 2);
							printf("buffer\n");
							bzero(data_buffer[i], 10000);
                        }

						while(operation == dtosm)
						{
							if(!finished_sending)
							{
								if(strlen(out_data_buffer + msg_position) <= MESSAGE_LENGTH - 1 - 4)
								{
									finished_sending = 1;
								}
								snprintf(out_buffer, finished_sending ? strlen(out_data_buffer + msg_position) + 4 + 1: MESSAGE_LENGTH, "3#%d#%s", finished_sending, out_data_buffer + msg_position);
								msg_position += MESSAGE_LENGTH - 1 - 4;

								/* clear the buffers and ready the SM */
								bzero(in_buffer, BUF_SIZE);
								printf("Sending to %d: %s\n", i, out_buffer);
								send(sd, out_buffer, strlen(out_buffer), 0);
								bzero(out_buffer, BUF_SIZE);
								read(sd, in_buffer, BUF_SIZE);
							}
							else
							{
								operation = -1;
							}
						}
						if(operation == -1) continue;

						/* receiving a result */
						if(operation == endagg)
						{
							group_results[n_groups - results_remaining] = atoi(strtok(NULL, "#"));
							results_remaining -= 1;
							if(results_remaining < 1)
							{
								stage = 0;
								has_generated_groups = 0;
							}
						}

						/* clear the buffers and ready the SM */
						bzero(in_buffer, BUF_SIZE);
						printf("Sending to %d: %s\n", i, out_buffer);
						send(sd, out_buffer, strlen(out_buffer), 0);
						bzero(out_buffer, BUF_SIZE);
						if((valread = read(sd, in_buffer, BUF_SIZE)) == 0)
						{
							getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
							printf("Host disconnected, ip %s, port %d.\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
							close(sd);
							client_socket[i] = 0;
						}
					}

					/* echo command */
					if(strncmp(cmd_buf, "echo", 4) == 0 && stage > 0){
						printf("Enter message for SM#%d: ", i);
						scanf("%s", out_buffer);
					}
					
					/* request command */
					else if(!strncmp(cmd_buf, "request", 7)){
						printf("RQ\n");
						if(stage == 100)
						{
							sprintf(out_buffer, "1#%d#%d", leadership[i], next_sms[i]);
						}
                        else if(!strncmp(data_buffer[i], "2", 1))
						{
                            sprintf(out_buffer, "%s", data_buffer[i]);
							printf("buffer\n");
							bzero(data_buffer[i], 10000);
                        }
						else
							sprintf(out_buffer, "5#ready, #%d.", i);
					} 
					
					/* wait */
					else
					{
						sprintf(out_buffer, "5#ready, #%d.", i);
					}
						
					/* transmit buffer contents */
					printf("Sending to #%d: %s\n", i, out_buffer);
					send(sd, out_buffer, strlen(out_buffer), 0);
					bzero(out_buffer, BUF_SIZE);			
				}
			}
		}

		/* make sure we have received all the responses */
		usleep(10);
		sleep_incr++;

		/* make sure all meters are registered / primed */
		
		if(num_sms > 0){
			continue;
		}
		if(num_sms == 0){
			num_sms = -1;
			continue;
		}
		

		/* handle operations */
		if(stage < 0)
		{
			stage += 1;
		} 
		
		else if(stage == 0)
		{
			if(n_groups){
				clock_gettime(CLOCK_MONOTONIC_RAW, &end);
				printf("Results: \n");
				for(int p=0; p<n_groups; p++){
					printf("Group %d: %d\n", p, group_results[p]);
				}
				uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
				printf("Time to aggregate: %llu\n", delta_us);
				printf("Sleeps %d\n", sleep_incr);
			}
			printf("Command: ");
			scanf("%s", cmd_buf);
			if(strncmp(cmd_buf, "echo", 7) == 0)
			{
				stage = 1;
			} 
			
			else if(strncmp(cmd_buf, "request", 7) == 0)
			{
				has_generated_leader = 0;
				results_remaining = num_sms_a / 4;
				n_groups = results_remaining;
				bzero(has_generated_next, 30);
				bzero(next_sms, 30);
				bzero(leadership, POP);
				stage = 100;
			}
		} 
		
		else
		{
			stage -= 1;
			printf("Stage: %d\n", stage);
		}
	}
	return 0;
}