while (event_count > 0 || !is_empty(&ready_queue))
    {
        Event current_event = extract_min(event_queue, &event_count);
        switch (current_event.type)
        {
            case 5:
                #ifdef VERBOSE
                    printf("%-10d: Process %d joins ready queue after timeout\n",current_event.time,current_event.process_id+1);
                #endif
                k=0;
            case 3:
                if(current_event.type==3)
                {
                    #ifdef VERBOSE
                        printf("%-10d: Process %d joins ready queue upon arrival\n",current_event.time,current_event.process_id+1);
                    #endif
                }
            case 0:
                enqueue(&ready_queue,current_event.process_id);    
                break;
        
            case 6:
                k=0;
                if(processes[current_event.process_id].bursts[processes[current_event.process_id].current_burst]==-1)
                {
                    #ifdef VERBOSE
                    printf("%-10d: Process      %d exits.\n",current_event.time,current_event.process_id+1);         
                    #endif
                    break;
                }
                Event arrival_event = {current_event.time+processes[current_event.process_id].bursts[processes[current_event.process_id].current_burst++], 1, current_event.process_id};
                insert_event(event_queue,&event_count,arrival_event);
                break; 

            case 1:
                #ifdef VERBOSE
                printf("%-10d: Process %d joins ready queue after IO completion\n",current_event.time,current_event.process_id+1);
                #endif
                enqueue(&ready_queue, current_event.process_id);
                break;

            case 4:
                processes[current_event.process_id].turnaround_time = current_event.time-processes[current_event.process_id].arrival_time;
                processes[current_event.process_id].wait_time =   current_event.time-processes[current_event.process_id].arrival_time-processes[current_event.process_id].burst_time;
                total_wait_time+=processes[current_event.process_id].wait_time;
                total_turnaround_time+=processes[current_event.process_id].turnaround_time;
                printf("%-10d: Process %6d exits. Turnaround time = %4d (%d%%), Wait time = %d\n",current_event.time,current_event.process_id+1,processes[current_event.process_id].turnaround_time,(int)(100.0*(processes[current_event.process_id].turnaround_time)/(processes[current_event.process_id].turnaround_time-processes[current_event.process_id].wait_time)+0.5),processes[current_event.process_id].wait_time);
                total_turnaround_time = current_event.time;
                k=0;
                break;
            default:
                break;
        }
        if (!is_empty(&ready_queue) && !k) 
        {
            if(p) total_idle_time+=current_event.time-idle_new;
            p=0;
            k=1;
            int next_process = dequeue(&ready_queue);
            processes[next_process].state = 6; // Running
            int burst_time = processes[next_process].bursts[processes[next_process].current_burst];
            if (burst_time > quantum) 
            {
                #ifdef VERBOSE
                    printf("%-10d: Process %d is scheduled to run for time %d\n",current_event.time,next_process+1,quantum);
                #endif
                Event timeout = {current_event.time + quantum, 5, next_process};
                insert_event(event_queue, &event_count, timeout);
                processes[next_process].bursts[processes[next_process].current_burst]-=quantum;
            } 
            else 
            {
                #ifdef VERBOSE
                    printf("%-10d: Process %d is scheduled to run for time %d\n",current_event.time,next_process+1,processes[next_process].bursts[processes[next_process].current_burst]);
                #endif
                if(processes[next_process].bursts[processes[next_process].current_burst+1]==-1)
                {
                    Event cpu_end = {current_event.time + burst_time, 4, next_process};
                    insert_event(event_queue, &event_count, cpu_end);

                }
                else
                {
                    Event cpu_end = {current_event.time + burst_time, 6, next_process};
                    insert_event(event_queue, &event_count, cpu_end);

                }
                processes[next_process].bursts[processes[next_process].current_burst]=0;
                processes[next_process].current_burst++;
            }
        }
        else if(!k)
        {
            idle_new = current_event.time;
            p=1;
            #ifdef VERBOSE
            printf("%-10d: CPU goes idle\n",current_event.time);
            #endif
        }
    }