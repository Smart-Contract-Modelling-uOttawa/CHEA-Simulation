#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <gmp.h>
#include <libhcs.h> 
#include <time.h>

#define PORT 8888
#define BUF_SIZE 60

enum operations {echo, measure, aggregate, dtosm, smtod, smwait, endagg};

struct SM
{
    int lat;
    int lon;
    char* id;
};

int serialize_SM(struct SM sm, char* sm_buf)
{
    char sm_buffer[1000];
    sprintf(sm_buffer, "%d$%d$%s", sm.lat, sm.lon, sm.id);
    strcpy(sm_buf, sm_buffer);
    return 0;
}

int main()
{

    /* initialize socket variables */
	int clientSocket, ret;
	struct sockaddr_in serverAddr;

    /* initialize buffers */
	char in_buffer[BUF_SIZE], out_buffer[BUF_SIZE];
    char in_data_buffer[10000] = "\0";
    char out_data_buffer[10000] = "\0";
	bzero(in_buffer, BUF_SIZE);
	bzero(out_buffer, BUF_SIZE);
    char sm_buf[1000];

    /* establish HE variables */
	pcs_private_key *vk;
	pcs_public_key *pk;
	hcs_random *hr;

    /* initialize plan variables */
	int leader = 0;
	int next_sm = 0;
	int measurement = 0;
    int msg_position = 0;
    int finished_sending = 0;
	mpz_t encdata_buffer;
	mpz_inits(encdata_buffer, NULL);

    /* initialize random for measurement */
	srand(getpid() % 100);


	struct timespec start, end;

    /* initialize SM details */
    //char* temp_id = "";
    //sprintf(temp_id, "%d", getpid());
    struct SM self = {rand() % 90, rand() % 90, "me"};
    //strcpy(self.id, temp_id);
    serialize_SM(self, sm_buf);
    printf("%s\n", sm_buf);

    /* END OF VARIABLES */

    /* OPERATIONS BEGIN */

    /* attempt to create client socket */
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(clientSocket < 0)
    {
		printf("[!]Connection Error[!]\n");
		exit(1);
	}

	printf("[+]Client socket created.\n");

	memset(&serverAddr, '\0', sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* attempt to connect to server */
	ret = connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if(ret < 0)
    {
		printf("[!]Connection Error[!]\n");
		exit(1);
	}
	printf("[+]Connected to server.\n");

    /* receive initial message */
	recv(clientSocket, in_buffer, BUF_SIZE, 0);
	printf("%s\n", in_buffer);
	bzero(in_buffer, BUF_SIZE);

    /* confirm connection */
	sprintf(out_buffer, "Registering...");
	send(clientSocket, out_buffer, strlen(out_buffer), 0);

    /* empty send buffer */
	bzero(out_buffer, BUF_SIZE);
 
    /* main loop */
	while(1)
    {
		printf("Waiting for server...\n");

        /* server closed */
		if(recv(clientSocket, in_buffer, BUF_SIZE, 0) < 0)
        {
			printf("[-]Server disconnected.\n");
			break;
		}

        /* handle incoming message */
        else
        {
			/* Handle the message. */
			printf("Data received: %s\n", in_buffer);

            /* get current operation instruction from server */
            enum operations operation = atoi(strtok(in_buffer, "#"));

            /* receiving data */
            if(operation == dtosm)
            {
                int finished_receiving = atoi(strtok(NULL, "#"));

                strcat(in_data_buffer, strtok(NULL, "#"));

                if(finished_receiving)
                {
                    printf("Data buffer contents: %s\n", in_data_buffer);
                    operation = aggregate;
                }
            }

            /* first step; measurement requested */
            if(operation == measure)
            {
                /* extract plan info */
                leader = atoi(strtok(NULL, "#"));
                next_sm = atoi(strtok(NULL, "#"));

                /* measure */
                measurement = rand() % 347;
                printf("\n[+]Measurement: %d\n\n", measurement);

                /* leader sends encrypted measurement & pk right away */
                if(leader)
                {
                    /* initialize HE */
                    pk = pcs_init_public_key();
                    vk = pcs_init_private_key();
                    hr = hcs_init_random();
                    pcs_generate_key_pair(pk, vk, hr, 2048);

                    /* encrypt measurement */
					mpz_t zmeasurement;
					mpz_inits(zmeasurement, NULL);
					mpz_set_ui(zmeasurement, measurement);
					pcs_encrypt(pk, hr, zmeasurement, zmeasurement);

					/* prep pk and measurement for send */
                    bzero(out_data_buffer, 10000);
                    sprintf(out_data_buffer, "2$%s$%s$%s$%s",
                    mpz_get_str(NULL, 62, pk->n), 
					mpz_get_str(NULL, 62, pk->g), 
					mpz_get_str(NULL, 62, pk->n2),
                    mpz_get_str(NULL, 62, zmeasurement));

					/* free memory */
					mpz_clears(zmeasurement, NULL);
					pcs_free_public_key(pk);
					hcs_free_random(hr);

                    /* set operation to data transfer */
                    operation = smtod;
                    finished_sending = 0;
				} 

                /* non-leaders just wait */
                else 
                {
					sprintf(out_buffer, "ready");
                }
            }

            /* main aggregation stage - members encrypt their */
            /* measurement with leader's pk and aggregate the result */
            else if(operation == aggregate && !leader)
            {
                clock_gettime(CLOCK_MONOTONIC_RAW, &start);
                /* initialize HE */
                pk = pcs_init_public_key();
                hr = hcs_init_random();
                /* extract pk and encrypted data received */
                //char*temp = strtok(in_data_buffer, "$");
                mpz_init_set_str(pk->n, strtok(in_data_buffer, "$"), 62);
                mpz_init_set_str(pk->g, strtok(NULL, "$"), 62);
                mpz_init_set_str(pk->n2, strtok(NULL, "$"), 62);
                mpz_init_set_str(encdata_buffer, strtok(NULL, "$"), 62);

                /* encrypt own measurement */
                mpz_t zmeasurement, total;
				mpz_inits(zmeasurement, total, NULL);
				mpz_set_ui(zmeasurement, measurement);
				pcs_encrypt(pk, hr, zmeasurement, zmeasurement);

                /* add own measurement to aggregate */
				pcs_ee_add(pk, total, encdata_buffer, zmeasurement);

                /* prep pk and data for send */
                bzero(out_data_buffer, 10000);
                sprintf(out_data_buffer, "2$%s$%s$%s$%s",
                mpz_get_str(NULL, 62, pk->n), 
				mpz_get_str(NULL, 62, pk->g), 
				mpz_get_str(NULL, 62, pk->n2),
                mpz_get_str(NULL, 62, total));
                printf("LENGTH OF MESSAGE: %lu\n", strlen(out_data_buffer));
                /* free memory */
                mpz_clears(zmeasurement, total, NULL);
                pcs_free_public_key(pk);
                hcs_free_random(hr);

                clock_gettime(CLOCK_MONOTONIC_RAW, &end);
                uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
				printf("Time to aggregate: %llu\n", delta_us);

                /* set operation to data transfer */
                operation = smtod;
                finished_sending = 0;
            }

            /* final step; receiving aggregate as leader */
            else if(operation == aggregate)
            {
                /* skip unnecessary info (pk) */
                char*temp = strtok(in_data_buffer, "$");
                temp = strtok(NULL, "$");
                temp = strtok(NULL, "$");

                /* extract encrypted aggregate */
                mpz_init_set_str(encdata_buffer, strtok(NULL, "$"), 62);
                gmp_printf("%Zd\n", encdata_buffer);

                /* decrypt aggregate */
                mpz_t result;
				mpz_inits(result, NULL);
				pcs_decrypt(vk, result, encdata_buffer);

                /* prep plaintext aggregate for send */
                sprintf(out_buffer, "6#%s", mpz_get_str(NULL, 10, result));

				/* free memory */
				mpz_clears(result, NULL);
				pcs_free_private_key(vk);
            }
			
            /* wait */
			else if(operation == smwait)
            {
				sprintf(out_buffer, "5#ready");
			}
			
			/* echo */
			else
            {
				sprintf(out_buffer, "0#(%s)", in_buffer);
			}

            /* sending data */
            if(operation == smtod)
            {
                if(!finished_sending)
                {
                    if(strlen(out_data_buffer + msg_position) <= BUF_SIZE - 1 - 4)
                    {
                        finished_sending = 1;
                        printf("Data buffer sent: %s\n", out_data_buffer);
                    }
                    snprintf(out_buffer, finished_sending ? strlen(out_data_buffer + msg_position) + 4 + 1 : BUF_SIZE, "4#%d#%s", finished_sending, out_data_buffer + msg_position);
                    msg_position += BUF_SIZE - 1 - 4;
                }
            }

            /* send message & clear buffers */
			printf("Sending data: %s\n", out_buffer);
			send(clientSocket, out_buffer, strlen(out_buffer), 0);
			bzero(out_buffer, BUF_SIZE);
			bzero(in_buffer, BUF_SIZE);
		}
	}

	return 0;
}