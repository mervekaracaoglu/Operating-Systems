#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>


#define BUFFER_SIZE 11

void print_dashes(int curDepth)
{
    for (int i = 0; i < curDepth; i++) 
    {
        printf("---");
    }
}


void treePipe(int curDepth, int maxDepth, int lr, int num1)
{
    int num2 = 1;
    int result;
    if(curDepth == maxDepth) // if leaf node
    {

        print_dashes(curDepth);
        printf("> My num1 is: %d\n", num1);

        write(STDOUT_FILENO, &num1, sizeof(int));
        write(STDOUT_FILENO, &num2, sizeof(int));
        
        if(lr == 0)
        {
            char *args[] = { "./left", NULL };
            execvp(args[0], args);
            perror("execvp failed for left child");
            exit(1);
        
        }
        else
        {
            char *args[] = { "./right", NULL };
            execvp(args[0], args);
            perror("execvp failed for right child");
            exit(1);
        }

    }
    else
    {

        int left_pipe[2];
        pipe(left_pipe);

        int left_child = fork();
        if(left_child < 0)
        {
            fprintf(stderr, "fork failed\n");
            exit(1);
        }
        if(left_child == 0)
        {
            dup2(left_pipe[1], STDOUT_FILENO);
            close(left_pipe[1]);
            
            dup2(left_pipe[0], STDIN_FILENO);
            close(left_pipe[0]);
    
            
            char depth_str[10], max_depth_str[10], lr_str[10];
            sprintf(depth_str, "%d", curDepth + 1);
            sprintf(max_depth_str, "%d", maxDepth);
            sprintf(lr_str, "%d", 0); 
        

            char *args[] = {"./treePipe", depth_str, max_depth_str, lr_str, NULL};
            execvp(args[0], args);
            perror("execvp failed");
            exit(1);

        }
        else
        {
    
            for(int i = curDepth; i <= maxDepth; i++)
            {
                print_dashes(i);
                printf("> Current depth: %d, lr: %d\n", i, lr);

            }
            print_dashes(maxDepth);
            printf("> My num1 is: %d\n", num1);

            close(left_pipe[0]);  // Close read end in parent

            write(left_pipe[1], &num1, sizeof(int));  // Send num1 to left child
            close(left_pipe[1]);


            waitpid(left_child, NULL, 0);


            int left_result;
            read(left_pipe[0], &left_result, sizeof(int));
            close(left_pipe[0]);


            print_dashes(curDepth);
            printf("> My result is: %d\n", left_result);

            num1 = left_result;

            int target_pipe[2];
            pipe(target_pipe);

            int target = fork();
            if(target < 0)
            {
                fprintf(stderr, "fork failed\n");
                exit(1);
            }
            else if(target == 0)
            {
                dup2(target_pipe[0], STDIN_FILENO);
                close(target_pipe[0]);

                dup2(target_pipe[1], STDOUT_FILENO);
                close(target_pipe[1]);

                char * l_or_r;
                if(lr == 0)
                {
                    l_or_r = "./left";
                }
                else
                {
                    l_or_r = "./right";
                }
                char *args[] ={l_or_r, NULL};
                execvp(l_or_r, args);
                printf("error");
                exit(1);
            }
            else
            {

                close(target_pipe[0]);  // Close read end in parent
                dprintf(target_pipe[1], "%d\n", left_result);  // Send `left_result` to target
                close(target_pipe[1]);

                waitpid(target, NULL, 0);
                
                read(target_pipe[0], &result, sizeof(int));
                close(target_pipe[0]);

                int right_pipe[2];
                pipe(right_pipe);

                close(right_pipe[0]);

                write(right_pipe[1], &result, sizeof(int));
                close(right_pipe[1]);

                int right_child = fork();
                if(right_child < 0)
                {
                    fprintf(stderr, "fork failed\n");
                    exit(1);
                }        
                else if(right_child == 0)
                {
                    close(right_pipe[1]);

                    dup2(right_pipe[0], STDIN_FILENO);
                    close(right_pipe[0]);
                    

                    char * l_or_r;
                    if(lr == 0)
                    {
                        l_or_r = "./left";
                    }
                    else
                    {
                        l_or_r = "./right";
                    }
                    char *args[] ={l_or_r, NULL};
                    execvp(l_or_r, args);
                    printf("error");
                    exit(1);

                }
                else
                {
                    close(right_pipe[0]);  // Close read end in parent
                    dprintf(right_pipe[1], "%d\n", result);  // Send `result` to right child
                    close(right_pipe[1]);

                    waitpid(right_child, NULL, 0);

                    read(STDIN_FILENO, &num2, sizeof(int));
                    

                    int final_result = num1 + num2;
                    dprintf(STDOUT_FILENO, "%d\n", final_result);
                

                }
            }
            
        }
    }
}


int main(int argc, char *argv[])
{

    if (argc != 4) 
    {
        fprintf(stderr, "Usage: %s <current depth> <max depth> <left/right indicator>\n", argv[0]);
        exit(1);
    }

    int num1;

    int curDepth = atoi(argv[1]);
    int maxDepth = atoi(argv[2]);
    int lr = atoi(argv[3]);



    if(curDepth == 0) //if root node
    {
        printf("> Current depth: %d, lr: %d\n", curDepth, lr);
        printf("Please enter num1 for the root: ");
        scanf("%d", &num1);;
    }


    int root_pipe[2];
    pipe(root_pipe);

    treePipe(curDepth, maxDepth, lr, num1);


    int final_result;
    read(root_pipe[0], &final_result, sizeof(final_result));
    printf("Final result: %d\n", final_result);

    close(root_pipe[0]);

    return 0;
}