/*
 * Copyright (c) 2008, Maxim Likhachev
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of Pennsylvania nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sbpl_arm_planner/headers.h>

#define PI_CONST 3.141592653
#define DEG2RAD(d) ((d)*(PI_CONST/180.0))
#define RAD2DEG(r) ((r)*(180.0/PI_CONST))

// cost multiplier
#define COSTMULT 1000

// distance from goal 
#define MAX_EUCL_DIJK_m .06

//number of elements in rotation matrix
#define SIZE_ROTATION_MATRIX 9

// discretization of joint angles
#define ANGLEDELTA 360.0

#define GRIPPER_LENGTH_M .06

#define OUTPUT_OBSTACLES 0
#define VERBOSE 1
#define OUPUT_DEGREES 1
#define DEBUG_MOTOR_LIMITS 0

// number of successors to each cell (for dyjkstra's heuristic)
#define DIRECTIONS 26

int dx[DIRECTIONS] = { 1,  1,  1,  0,  0,  0, -1, -1, -1,    1,  1,  1,  0,  0, -1, -1, -1,    1,  1,  1,  0,  0,  0, -1, -1, -1};
int dy[DIRECTIONS] = { 1,  0, -1,  1,  0, -1, -1,  0,  1,    1,  0, -1,  1, -1, -1,  0,  1,    1,  0, -1,  1,  0, -1, -1,  0,  1};
int dz[DIRECTIONS] = {-1, -1, -1, -1, -1, -1, -1, -1, -1,    0,  0,  0,  0,  0,  0,  0,  0,    1,  1,  1,  1,  1,  1,  1,  1,  1};

static clock_t DH_time = 0;
static clock_t KL_time = 0;
static clock_t check_collision_time = 0;
static int num_forwardkinematics = 0;
// static clock_t time_gethash = 0;
// static int num_GetHashEntry = 0;


/*------------------------------------------------------------------------*/
                        /* State Access Functions */
/*------------------------------------------------------------------------*/
/**
 * @brief hash function (just testing doxygen....)
*/
static unsigned int inthash(unsigned int key)
{
  key += (key << 12); 
  key ^= (key >> 22);
  key += (key << 4);
  key ^= (key >> 9);
  key += (key << 10);
  key ^= (key >> 2);
  key += (key << 7);
  key ^= (key >> 12);
  return key;
}

unsigned int EnvironmentROBARM3D::GETHASHBIN(short unsigned int* coord, int numofcoord)
{

    int val = 0;

    for(int i = 0; i < numofcoord; i++)
    {
        val += inthash(coord[i]) << i;
    }

    return inthash(val) & (EnvROBARM.HashTableSize-1);
}

void EnvironmentROBARM3D::PrintHashTableHist()
{
    int s0=0, s1=0, s50=0, s100=0, s200=0, s300=0, slarge=0;

    for(int  j = 0; j < EnvROBARM.HashTableSize; j++)
    {
        if((int)EnvROBARM.Coord2StateIDHashTable[j].size() == 0)
                s0++;
        else if((int)EnvROBARM.Coord2StateIDHashTable[j].size() < 50)
                s1++;
        else if((int)EnvROBARM.Coord2StateIDHashTable[j].size() < 100)
                s50++;
        else if((int)EnvROBARM.Coord2StateIDHashTable[j].size() < 200)
                s100++;
        else if((int)EnvROBARM.Coord2StateIDHashTable[j].size() < 300)
                s200++;
        else if((int)EnvROBARM.Coord2StateIDHashTable[j].size() < 400)
                s300++;
        else
                slarge++;
    }
    printf("hash table histogram: 0:%d, <50:%d, <100:%d, <200:%d, <300:%d, <400:%d >400:%d\n",
        s0,s1, s50, s100, s200,s300,slarge);
}

EnvROBARMHashEntry_t* EnvironmentROBARM3D::GetHashEntry(short unsigned int* coord, int numofcoord, short unsigned int action, bool bIsGoal)
{
//     num_GetHashEntry++;
//     clock_t currenttime = clock();

    //if it is goal
    if(bIsGoal)
    {
        return EnvROBARM.goalHashEntry;
    }

    int binid = GETHASHBIN(coord, numofcoord);

#if DEBUG
    if ((int)EnvROBARM.Coord2StateIDHashTable[binid].size() > 500)
    {
	printf("WARNING: Hash table has a bin %d (coord0=%d) of size %d\n", 
            binid, coord[0], EnvROBARM.Coord2StateIDHashTable[binid].size());
	PrintHashTableHist();
    }
#endif

    //iterate over the states in the bin and select the perfect match
    for(int ind = 0; ind < (int)EnvROBARM.Coord2StateIDHashTable[binid].size(); ind++)
    {
        int j = 0;

        if(EnvROBARMCfg.use_smooth_actions)
        {
            //first check if it is the correct action
            if (EnvROBARM.Coord2StateIDHashTable[binid][ind]->action != action)
                continue;
        }

        for(j = 0; j < numofcoord; j++)
        {
            if(EnvROBARM.Coord2StateIDHashTable[binid][ind]->coord[j] != coord[j]) 
            {
                break;
            }
        }

        if (j == numofcoord)
        {
//             time_gethash += clock()-currenttime;
            return EnvROBARM.Coord2StateIDHashTable[binid][ind];
        }
    }

//     time_gethash += clock()-currenttime;

    return NULL;
}

EnvROBARMHashEntry_t* EnvironmentROBARM3D::CreateNewHashEntry(short unsigned int* coord, int numofcoord, short unsigned int endeff[3], short unsigned int action, double orientation[3][3])
{
    int i;
    //clock_t currenttime = clock();
    EnvROBARMHashEntry_t* HashEntry = new EnvROBARMHashEntry_t;

    memcpy(HashEntry->coord, coord, numofcoord*sizeof(short unsigned int));
    memcpy(HashEntry->endeff, endeff, 3*sizeof(short unsigned int));

    if(orientation != NULL)
        memcpy(HashEntry->orientation, orientation, 9*sizeof(double));

    HashEntry->action = action;

    // assign a stateID to HashEntry to be used 
    HashEntry->stateID = EnvROBARM.StateID2CoordTable.size();

    //insert into the tables
    EnvROBARM.StateID2CoordTable.push_back(HashEntry);

    //get the hash table bin
    i = GETHASHBIN(HashEntry->coord, numofcoord);

    //insert the entry into the bin
    EnvROBARM.Coord2StateIDHashTable[i].push_back(HashEntry);

    //insert into and initialize the mappings
    int* entry = new int [NUMOFINDICES_STATEID2IND];
    StateID2IndexMapping.push_back(entry);
    for(i = 0; i < NUMOFINDICES_STATEID2IND; i++)
    {
        StateID2IndexMapping[HashEntry->stateID][i] = -1;
    }

    if(HashEntry->stateID != (int)StateID2IndexMapping.size()-1)
    {
        printf("ERROR in Env... function: last state has incorrect stateID\n");
        exit(1);	
    }

    //time_createhash += clock()-currenttime;
    return HashEntry;
}
//--------------------------------------------------------------


/*------------------------------------------------------------------------*/
                       /* Heuristic Computation */
/*------------------------------------------------------------------------*/
//compute straight line distance from x,y,z to every other cell
void EnvironmentROBARM3D::getDistancetoGoal(int* HeurGrid, int goalx, int goaly, int goalz)
{
    for(int x = 0; x < EnvROBARMCfg.EnvWidth_c; x++)
    {
        for(int y = 0; y < EnvROBARMCfg.EnvHeight_c; y++)
        {
            for(int z = 0; z < EnvROBARMCfg.EnvDepth_c; z++)
                HeurGrid[XYZTO3DIND(x,y,z)] = (EnvROBARMCfg.cost_per_cell)*sqrt((goalx-x)*(goalx-x) + (goaly-y)*(goaly-y) +  (goalz-z)*(goalz-z));
        }
    }
}

int EnvironmentROBARM3D::XYZTO3DIND(int x, int y, int z)
{
    if(EnvROBARMCfg.lowres_collision_checking)
        return ((x) + (y)*EnvROBARMCfg.LowResEnvWidth_c + (z)*EnvROBARMCfg.LowResEnvWidth_c*EnvROBARMCfg.LowResEnvHeight_c);
    else
        return ((x) + (y)*EnvROBARMCfg.EnvWidth_c + (z)*EnvROBARMCfg.EnvWidth_c*EnvROBARMCfg.EnvHeight_c);
}

void EnvironmentROBARM3D::ReInitializeState3D(State3D* state)
{
	state->g = INFINITECOST;
	state->iterationclosed = 0;
}

void EnvironmentROBARM3D::InitializeState3D(State3D* state, short unsigned int x, short unsigned int y, short unsigned int z)
{
    state->g = INFINITECOST;
    state->iterationclosed = 0;
    state->x = x;
    state->y = y;
    state->z = z;
}

void EnvironmentROBARM3D::Create3DStateSpace(State3D**** statespace3D)
{
    int x,y,z;
    int width = EnvROBARMCfg.EnvWidth_c;
    int height = EnvROBARMCfg.EnvHeight_c;
    int depth = EnvROBARMCfg.EnvDepth_c;
    
    if(EnvROBARMCfg.lowres_collision_checking)
    {
        width = EnvROBARMCfg.LowResEnvWidth_c;
        height = EnvROBARMCfg.LowResEnvHeight_c;
        depth = EnvROBARMCfg.LowResEnvDepth_c;
    }
    
    //allocate a statespace for 3D search
    *statespace3D = new State3D** [width];
    for (x = 0; x < width; x++)
    {
        (*statespace3D)[x] = new State3D* [height];
        for(y = 0; y < height; y++)
        {
            (*statespace3D)[x][y] = new State3D [depth];
            for(z = 0; z < depth; z++)
            {
                InitializeState3D(&(*statespace3D)[x][y][z],x,y,z);
            }
        }
    }
}

void EnvironmentROBARM3D::Delete3DStateSpace(State3D**** statespace3D)
{
    int x,y;
    int width = EnvROBARMCfg.EnvWidth_c;
    int height = EnvROBARMCfg.EnvHeight_c;

    if(EnvROBARMCfg.lowres_collision_checking)
    {
        width = EnvROBARMCfg.LowResEnvWidth_c;
        height = EnvROBARMCfg.LowResEnvHeight_c;
    }

	//delete the 3D statespace
	for (x = 0; x < width; x++)
	{
	   for (y = 0; y < height; y++)
		delete [] (*statespace3D)[x][y];

	   delete [] (*statespace3D)[x];
	}
	delete *statespace3D;
}

void EnvironmentROBARM3D::ComputeHeuristicValues()
{
    printf("Running 3D BFS to compute heuristics\n");
    clock_t currenttime = clock();
    int hsize;

    if(EnvROBARMCfg.lowres_collision_checking)
        hsize = XYZTO3DIND(EnvROBARMCfg.LowResEnvWidth_c-1, EnvROBARMCfg.LowResEnvHeight_c-1,EnvROBARMCfg.LowResEnvDepth_c-1)+1;
    else
        hsize = XYZTO3DIND(EnvROBARMCfg.EnvWidth_c-1, EnvROBARMCfg.EnvHeight_c-1,EnvROBARMCfg.EnvDepth_c-1)+1;

    //allocate memory
    EnvROBARM.Heur = new int [hsize]; 

    //now compute the heuristics for all of the goal locations
    State3D*** statespace3D;	
    Create3DStateSpace(&statespace3D);

    Search3DwithQueue(statespace3D, EnvROBARM.Heur, EnvROBARMCfg.EndEffGoals_c);
//     Search3DwithQueue(statespace3D, EnvROBARM.Heur, EnvROBARMCfg.EndEffGoalX_c, EnvROBARMCfg.EndEffGoalY_c, EnvROBARMCfg.EndEffGoalZ_c);
//     Search3DwithHeap(statespace3D, EnvROBARM.Heur, EnvROBARMCfg.EndEffGoalX_c, EnvROBARMCfg.EndEffGoalY_c, EnvROBARMCfg.EndEffGoalZ_c);

    Delete3DStateSpace(&statespace3D);
    printf("completed in %.3f seconds.\n", double(clock()-currenttime) / CLOCKS_PER_SEC);
}

void EnvironmentROBARM3D::Search3DwithQueue(State3D*** statespace, int* HeurGrid, short unsigned int searchstartx, short unsigned int searchstarty, short unsigned int searchstartz)
{
    State3D* ExpState;
    int newx, newy, newz, x,y,z;
    unsigned int g_temp;
    char*** Grid3D = EnvROBARMCfg.Grid3D;
    short unsigned int width = EnvROBARMCfg.EnvWidth_c;
    short unsigned int height = EnvROBARMCfg.EnvHeight_c;
    short unsigned int depth = EnvROBARMCfg.EnvDepth_c;

    //use low resolution grid if enabled
    if(EnvROBARMCfg.lowres_collision_checking)
    {
        searchstartx *= (EnvROBARMCfg.GridCellWidth / EnvROBARMCfg.LowResGridCellWidth);
        searchstarty *= (EnvROBARMCfg.GridCellWidth / EnvROBARMCfg.LowResGridCellWidth);
        searchstartz *= (EnvROBARMCfg.GridCellWidth / EnvROBARMCfg.LowResGridCellWidth);

        width = EnvROBARMCfg.LowResEnvWidth_c;
        height = EnvROBARMCfg.LowResEnvHeight_c;
        depth = EnvROBARMCfg.LowResEnvDepth_c;

        Grid3D = EnvROBARMCfg.LowResGrid3D;
    }

    //create a queue
    queue<State3D*> Queue; 

    int b = 0;
    //initialize to infinity all 
    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {
            for (z = 0; z < depth; z++)
            {
                HeurGrid[XYZTO3DIND(x,y,z)] = INFINITECOST;
                ReInitializeState3D(&statespace[x][y][z]);
                b++;
            }
        }
    }

    //initialization
    statespace[searchstartx][searchstarty][searchstartz].g = 0;	
    Queue.push(&statespace[searchstartx][searchstarty][searchstartz]);

    //expand all of the states
    while((int)Queue.size() > 0)
    {
        //get the state to expand
        ExpState = Queue.front();

        Queue.pop();

        //it may be that the state is already closed
        if(ExpState->iterationclosed == 1)
            continue;

        //close it
        ExpState->iterationclosed = 1;

        //set the corresponding heuristics
        HeurGrid[XYZTO3DIND(ExpState->x, ExpState->y, ExpState->z)] = ExpState->g;

        //iterate through neighbors
        for(int d = 0; d < DIRECTIONS; d++)
        {
            newx = ExpState->x + dx[d];
            newy = ExpState->y + dy[d];
            newz = ExpState->z + dz[d];

            //make sure it is inside the map and has no obstacle
//             if(0 > newx || newx >= width || 0 > newy || newy >= height || 0 > newz || newz >= depth || Grid3D[newx][newy][newz] == 1)
            if(0 > newx || newx >= width || 0 > newy || newy >= height || 0 > newz || newz >= depth || Grid3D[newx][newy][newz] >= EnvROBARMCfg.ObstacleCost)
            {
                continue;
            }

            if(statespace[newx][newy][newz].iterationclosed == 0)
            {
                //insert into the stack
                Queue.push(&statespace[newx][newy][newz]);

                //set the g-value
                if (ExpState->x != newx && ExpState->y != newy && ExpState->z != newz)
                    g_temp = ExpState->g + EnvROBARMCfg.cost_sqrt3_move;
                else if ((ExpState->y != newy && ExpState->z != newz) ||
                        (ExpState->x != newx && ExpState->z != newz) ||
                        (ExpState->x != newx && ExpState->y != newy))
                    g_temp = ExpState->g + EnvROBARMCfg.cost_sqrt2_move;
                else
                    g_temp = ExpState->g + EnvROBARMCfg.cost_per_cell;

                if(statespace[newx][newy][newz].g > g_temp)
                    statespace[newx][newy][newz].g = g_temp;
//                 printf("%i: %u,%u,%u, g-cost: %i\n",d,newx,newy,newz,statespace[newx][newy][newz].g);
            }
        }
    }
}

void EnvironmentROBARM3D::Search3DwithQueue(State3D*** statespace, int* HeurGrid, short unsigned int ** EndEffGoals_c)
{
    State3D* ExpState;
    int newx, newy, newz, x,y,z;
    short unsigned int goal_xyz[3];
    unsigned int g_temp;
    char*** Grid3D = EnvROBARMCfg.Grid3D;
    short unsigned int width = EnvROBARMCfg.EnvWidth_c;
    short unsigned int height = EnvROBARMCfg.EnvHeight_c;
    short unsigned int depth = EnvROBARMCfg.EnvDepth_c;

    //use low resolution grid if enabled
    if(EnvROBARMCfg.lowres_collision_checking)
    {
        width = EnvROBARMCfg.LowResEnvWidth_c;
        height = EnvROBARMCfg.LowResEnvHeight_c;
        depth = EnvROBARMCfg.LowResEnvDepth_c;

        Grid3D = EnvROBARMCfg.LowResGrid3D;
    }

    //create a queue
    queue<State3D*> Queue; 

    int b = 0;
    //initialize to infinity all 
    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {
            for (z = 0; z < depth; z++)
            {
                HeurGrid[XYZTO3DIND(x,y,z)] = INFINITECOST;
                ReInitializeState3D(&statespace[x][y][z]);
                b++;
            }
        }
    }

    //initialization - throw starting states on queue with g cost = 0
    for(int i = 0; i < EnvROBARMCfg.nEndEffGoals; i++)
    {
        goal_xyz[0] = EndEffGoals_c[i][0];
        goal_xyz[1] = EndEffGoals_c[i][1];
        goal_xyz[2] = EndEffGoals_c[i][2];

        if(EnvROBARMCfg.lowres_collision_checking)
        {
            goal_xyz[0] *= (EnvROBARMCfg.GridCellWidth / EnvROBARMCfg.LowResGridCellWidth);
            goal_xyz[1] *= (EnvROBARMCfg.GridCellWidth / EnvROBARMCfg.LowResGridCellWidth);
            goal_xyz[2] *= (EnvROBARMCfg.GridCellWidth / EnvROBARMCfg.LowResGridCellWidth);
        }

        statespace[goal_xyz[0]][goal_xyz[1]][goal_xyz[2]].g = 0;
        Queue.push(&statespace[goal_xyz[0]][goal_xyz[1]][goal_xyz[2]]);
    }

    //expand all of the states
    while((int)Queue.size() > 0)
    {
        //get the state to expand
        ExpState = Queue.front();

        Queue.pop();

        //it may be that the state is already closed
        if(ExpState->iterationclosed == 1)
            continue;

        //close it
        ExpState->iterationclosed = 1;

        //set the corresponding heuristics
        HeurGrid[XYZTO3DIND(ExpState->x, ExpState->y, ExpState->z)] = ExpState->g;

        //iterate through neighbors
        for(int d = 0; d < DIRECTIONS; d++)
        {
            newx = ExpState->x + dx[d];
            newy = ExpState->y + dy[d];
            newz = ExpState->z + dz[d];

            //make sure it is inside the map and has no obstacle
            if(0 > newx || newx >= width || 0 > newy || newy >= height || 0 > newz || newz >= depth || Grid3D[newx][newy][newz] >= EnvROBARMCfg.ObstacleCost)
            {
                continue;
            }

            if(statespace[newx][newy][newz].iterationclosed == 0)
            {
                //insert into the stack
                Queue.push(&statespace[newx][newy][newz]);

                //set the g-value
                if (ExpState->x != newx && ExpState->y != newy && ExpState->z != newz)
                    g_temp = ExpState->g + EnvROBARMCfg.cost_sqrt3_move;
                else if ((ExpState->y != newy && ExpState->z != newz) ||
                          (ExpState->x != newx && ExpState->z != newz) ||
                          (ExpState->x != newx && ExpState->y != newy))
                    g_temp = ExpState->g + EnvROBARMCfg.cost_sqrt2_move;
                else
                    g_temp = ExpState->g + EnvROBARMCfg.cost_per_cell;

                if(statespace[newx][newy][newz].g > g_temp)
                    statespace[newx][newy][newz].g = g_temp;
//                 printf("%i: %u,%u,%u, g-cost: %i\n",d,newx,newy,newz,statespace[newx][newy][newz].g);
            }
        }
    }
}

/*
void EnvironmentROBARM3D::Search3DwithHeap(State3D*** statespace, int* HeurGrid, int searchstartx, int searchstarty, int searchstartz)
{
    CKey key;	
    CHeap* heap;
    State3D* ExpState;
    int newx, newy, newz, x,y,z;
    unsigned int g_temp;

    //create a heap
    heap = new CHeap; 

    //initialize to infinity all 
    for (x = 0; x < EnvROBARMCfg.EnvWidth_c; x++)
    {
        for (y = 0; y < EnvROBARMCfg.EnvHeight_c; y++)
        {
            for (z = 0; z < EnvROBARMCfg.EnvDepth_c; z++)
            {
                HeurGrid[XYZTO3DIND(x,y,z)] = INFINITECOST;
                ReInitializeState3D(&statespace[x][y][z]);
            }
        }
    }

    //initialization
    statespace[searchstartx][searchstarty][searchstartz].g = 0;
    key.key[0] = 0;
    heap->insertheap(&statespace[searchstartx][searchstarty][searchstartz], key);


    //expand all of the states
    while(!heap->emptyheap())
    {
        //get the state to expand
        ExpState = heap->deleteminheap();

        //set the corresponding heuristics
        HeurGrid[XYZTO3DIND(ExpState->x, ExpState->y, ExpState->z)] = ExpState->g;

        //iterate through neighbors
        for(int d = 0; d < DIRECTIONS; d++)
        {
            newx = ExpState->x + dx[d];
            newy = ExpState->y + dy[d];
            newz = ExpState->z + dz[d];

            //make sure it is inside the map and has no obstacle
            if(0 > newx || newx >= EnvROBARMCfg.EnvWidth_c ||
                0 > newy || newy >= EnvROBARMCfg.EnvHeight_c ||
                0 > newz || newz >= EnvROBARMCfg.EnvDepth_c ||
                EnvROBARMCfg.Grid3D[newx][newy][newz] >= EnvROBARMCfg.ObstacleCost)
                    continue;

            if(statespace[newx][newy][newz].g > ExpState->g + 1 &&
                    EnvROBARMCfg.Grid3D[newx][newy][newz] == 0)
            {
                statespace[newx][newy][newz].g = ExpState->g + 1;
                key.key[0] = statespace[newx][newy][newz].g;
                if(statespace[newx][newy][newz].heapindex == 0)
                    heap->insertheap(&statespace[newx][newy][newz], key);
                else
                    heap->updateheap(&statespace[newx][newy][newz], key);
            }

        }
    }

    //delete the heap	
    delete heap;
}

//--------------------------------------------------------------
*/


/*------------------------------------------------------------------------*/
                        /* Domain Specific Functions */
/*------------------------------------------------------------------------*/
void EnvironmentROBARM3D::ReadConfiguration(FILE* fCfg)
{
    char sTemp[1024];
    int x,i;
    double obs[6];

    fscanf(fCfg, "%s", sTemp);
    while(!feof(fCfg) && strlen(sTemp) != 0)
    {
        if(strcmp(sTemp, "environmentsize(meters):") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.EnvWidth_m = atof(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.EnvHeight_m = atof(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.EnvDepth_m = atof(sTemp);
        }
        else if(strcmp(sTemp, "discretization(cells):") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.EnvWidth_c = atoi(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.EnvHeight_c = atoi(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.EnvDepth_c = atoi(sTemp);

            //Low-Res Environment for Colision Checking
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.LowResEnvWidth_c = atoi(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.LowResEnvHeight_c = atoi(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.LowResEnvDepth_c = atoi(sTemp);

            //set additional parameters
            EnvROBARMCfg.GridCellWidth = EnvROBARMCfg.EnvWidth_m/EnvROBARMCfg.EnvWidth_c;
            EnvROBARMCfg.LowResGridCellWidth = EnvROBARMCfg.EnvWidth_m/EnvROBARMCfg.LowResEnvWidth_c;

            //temporary - I have no clue why the commented line doesn't work
            double a = (EnvROBARMCfg.EnvWidth_m / double(EnvROBARMCfg.EnvWidth_c));
            double b = (EnvROBARMCfg.EnvHeight_m / double(EnvROBARMCfg.EnvHeight_c));
            double c = (EnvROBARMCfg.EnvDepth_m / double(EnvROBARMCfg.EnvDepth_c));

            //WTF!?!?!? if you comment out the printf statement it doesnt work!?
            printf("%f %f %f\n",a,b,c);

            if(a != b || a != c || b != c)
            //if((EnvROBARMCfg.EnvWidth_m / double(EnvROBARMCfg.EnvWidth_c)) != (EnvROBARMCfg.EnvHeight_m / double(EnvROBARMCfg.EnvHeight_c)))
            //   || EnvROBARMCfg.GridCellWidth != EnvROBARMCfg.EnvDepth_m/EnvROBARMCfg.EnvDepth_c)
            {
                printf("ERROR: The cell should be square\n");
                exit(1);
            }

            //allocate memory for environment grid
            InitializeEnvGrid();
        }
        else if(strcmp(sTemp, "basexyz(cells):") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.BaseX_c = atoi(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.BaseY_c = atoi(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.BaseZ_c = atoi(sTemp);

            //convert shoulder base from cell to real world coords
            Cell2ContXYZ(EnvROBARMCfg.BaseX_c, EnvROBARMCfg.BaseY_c, EnvROBARMCfg.BaseZ_c, &(EnvROBARMCfg.BaseX_m), &(EnvROBARMCfg.BaseY_m), &(EnvROBARMCfg.BaseZ_m)); 
        }
        else if(strcmp(sTemp, "basexyz(meters):") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.BaseX_m = atof(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.BaseY_m = atof(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.BaseZ_m = atof(sTemp);
            ContXYZ2Cell(EnvROBARMCfg.BaseX_m, EnvROBARMCfg.BaseY_m, EnvROBARMCfg.BaseZ_m, &(EnvROBARMCfg.BaseX_c), &(EnvROBARMCfg.BaseY_c), &(EnvROBARMCfg.BaseZ_c)); 
        }
        else if(strcmp(sTemp, "linklengths(meters):") == 0)
        {
            for(i = 0; i < NUMOFLINKS; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.LinkLength_m[i] = atof(sTemp);
            }
        }
        else if(strcmp(sTemp, "linkstartangles(degrees):") == 0)
        {
            for(i = 0; i < NUMOFLINKS; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.LinkStartAngles_d[i] = atoi(sTemp);
            }
        }
        else if (strcmp(sTemp, "endeffectorgoal(cells):") == 0)
        {
            //only endeffector is specified
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.EndEffGoalX_c = atoi(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.EndEffGoalY_c = atoi(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.EndEffGoalZ_c = atoi(sTemp);

            //set goalangle to invalid number
            EnvROBARMCfg.LinkGoalAngles_d[0] = INVALID_NUMBER;
        }
        else if(strcmp(sTemp, "linkgoalangles(degrees):") == 0)
        {
            for(i = 0; i < NUMOFLINKS; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.LinkGoalAngles_d[i] = atoi(sTemp);
            }

            //so the goal's location can be calculated after the initialization completes
            EnvROBARMCfg.JointSpaceGoal = 1;
        }
        else if(strcmp(sTemp, "linkgoalangles(radians):") == 0)
        {
            for(i = 0; i < NUMOFLINKS; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.LinkGoalAngles_d[i] = RAD2DEG(atof(sTemp));
            }

                //so the goal's location can be calculated after the initialization completes
            EnvROBARMCfg.JointSpaceGoal = 1;
        }
        else if(strcmp(sTemp, "endeffectorgoal(meters):") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.nEndEffGoals = atoi(sTemp);

            //allocate memory
            EnvROBARMCfg.EndEffGoals_m = new double * [EnvROBARMCfg.nEndEffGoals];
            for (i = 0; i < EnvROBARMCfg.nEndEffGoals; i++)
                EnvROBARMCfg.EndEffGoals_m[i] = new double [12];

            for(i = 0; i < EnvROBARMCfg.nEndEffGoals; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                if(strcmp(sTemp,"linktwist(degrees):") != 0)
                {
                    EnvROBARMCfg.EndEffGoals_m[i][0] = atof(sTemp);

                    for(int k = 1; k < 12; k++)
                    {
                        fscanf(fCfg, "%s", sTemp);
                        EnvROBARMCfg.EndEffGoals_m[i][k] = atof(sTemp);
                    }
                }
                else
                {
                    printf("Fewer goal positions in environment file than stated.\n");
                    exit(1);
                }
            }
        }
        else if(strcmp(sTemp, "endeffectorgoal(meters-rpy):") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.nEndEffGoals = atoi(sTemp);

            //allocate memory
            EnvROBARMCfg.EndEffGoals_m = new double * [EnvROBARMCfg.nEndEffGoals];
            for (i = 0; i < EnvROBARMCfg.nEndEffGoals; i++)
                EnvROBARMCfg.EndEffGoals_m[i] = new double [6];

            for(i = 0; i < EnvROBARMCfg.nEndEffGoals; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                if(strcmp(sTemp,"linktwist(degrees):") != 0)
                {
                    EnvROBARMCfg.EndEffGoals_m[i][0] = atof(sTemp);

                    for(int k = 1; k < 6; k++)
                    {
                        fscanf(fCfg, "%s", sTemp);
                        EnvROBARMCfg.EndEffGoals_m[i][k] = atof(sTemp);
                    }
                }
                else
                {
                    printf("Fewer goal positions in environment file than stated.\n");
                    exit(1);
                }
            }
        }
        else if(strcmp(sTemp, "linktwist(degrees):") == 0)
        {
            for(i = 0; i < NUMOFLINKS_DH; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.DH_alpha[i] = PI_CONST*(atof(sTemp)/180.0);
            }
        }
        else if(strcmp(sTemp, "linklength(meters):") == 0)
        {
            for(i = 0; i < NUMOFLINKS_DH; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.DH_a[i] = atof(sTemp);
                EnvROBARMCfg.LinkLength_m[i] = atof(sTemp);
            }
        }
        else if(strcmp(sTemp, "linkoffset(meters):") == 0)
        {
            for(i = 0; i < NUMOFLINKS_DH; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.DH_d[i] = atof(sTemp);
            }
        }
        else if(strcmp(sTemp, "jointangle(degrees):") == 0)
        {
            for(i = 0; i < NUMOFLINKS_DH; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.DH_theta[i] = DEG2RAD(atof(sTemp));
            }
        }
        else if(strcmp(sTemp,"posmotorlimits(degrees):") == 0)
        {
            for(i = 0; i < NUMOFLINKS; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.PosMotorLimits[i] = DEG2RAD(atof(sTemp));
            }
        }
        else if(strcmp(sTemp,"posmotorlimits(radians):") == 0)
        {
            for(i = 0; i < NUMOFLINKS; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.PosMotorLimits[i] = atof(sTemp);
            }
        }
        else if(strcmp(sTemp,"negmotorlimits(degrees):") == 0)
        {
            for(i = 0; i < NUMOFLINKS; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.NegMotorLimits[i] = DEG2RAD(atof(sTemp));
            }
        }
        else if(strcmp(sTemp,"negmotorlimits(radians):") == 0)
        {
            for(i = 0; i < NUMOFLINKS; i++)
            {
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.NegMotorLimits[i] = atof(sTemp);
            }
        }
        else if(strcmp(sTemp, "goalorientationMOE:") == 0)
        {
            for(x = 0; x < 3; x++)
            {
                //acceptable margin of error of that row
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.GoalOrientationMOE[x][0] = atof(sTemp);
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.GoalOrientationMOE[x][1] = atof(sTemp);
                fscanf(fCfg, "%s", sTemp);
                EnvROBARMCfg.GoalOrientationMOE[x][2] = atof(sTemp);
            }
        }
        else if(strcmp(sTemp, "goalorientationMOE-RPY:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.GoalRPY_MOE[0] = atof(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.GoalRPY_MOE[1] = atof(sTemp);
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.GoalRPY_MOE[2] = atof(sTemp);
        }
        else if(strcmp(sTemp, "environment:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            while(!feof(fCfg) && strlen(sTemp) != 0)
            {
                if(strcmp(sTemp, "cube(meters):") == 0)
                {
                    for(i = 0; i < 6; i++)
                    {
                        fscanf(fCfg, "%s", sTemp);
                        obs[i] = atof(sTemp);
                    }
                    AddObstacleToGrid(obs, 0, EnvROBARMCfg.Grid3D, EnvROBARMCfg.GridCellWidth);

                    if(EnvROBARMCfg.lowres_collision_checking)
                        AddObstacleToGrid(obs, 0, EnvROBARMCfg.LowResGrid3D, EnvROBARMCfg.LowResGridCellWidth);
                }
                else if (strcmp(sTemp, "cube(cells):") == 0)
                {
                    for(i = 0; i < 6; i++)
                    {
                        fscanf(fCfg, "%s", sTemp);
                        obs[i] = atof(sTemp);
                    }
                    AddObstacleToGrid(obs, 1, EnvROBARMCfg.Grid3D, EnvROBARMCfg.GridCellWidth);

                    if(EnvROBARMCfg.lowres_collision_checking)
                        AddObstacleToGrid(obs, 1, EnvROBARMCfg.LowResGrid3D, EnvROBARMCfg.LowResGridCellWidth);
                }
                else
                {
                    printf("ERROR: Environment Config file contains unknown object type.\n");
                    exit(1);
                }
                fscanf(fCfg, "%s", sTemp);
            }
        }
        else
        {
            printf("ERROR: Unknown parameter name in environment config file: %s.\n", sTemp);
        }
        fscanf(fCfg, "%s", sTemp);
    }
    printf("Config file successfully read.\n");
}

//parse algorithm parameter file
void EnvironmentROBARM3D::ReadParamsFile(FILE* fCfg)
{
    char sTemp[1024];
    int x,y,nrows,ncols;

    fscanf(fCfg, "%s", sTemp);
    while(!feof(fCfg) && strlen(sTemp) != 0)
    {
        if(strcmp(sTemp, "Use_DH_for_FK:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.use_DH = atoi(sTemp);
        }
        else if(strcmp(sTemp, "Enforce_Motor_Limits:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.enforce_motor_limits = atoi(sTemp);
        }
        else if(strcmp(sTemp, "Use_Dijkstra_for_Heuristic:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.dijkstra_heuristic = atoi(sTemp);
        }
        else if(strcmp(sTemp, "Collision_Checking_on_EndEff_only:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.endeff_check_only = atoi(sTemp);
        }
        else if(strcmp(sTemp, "Obstacle_Padding(meters):") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.padding = atof(sTemp);
        }
        else if(strcmp(sTemp, "Smoothing_Weight:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.smoothing_weight = atof(sTemp);
        }
        else if(strcmp(sTemp, "Use_Path_Smoothing:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.use_smooth_actions = atoi(sTemp);
        }
        else if(strcmp(sTemp, "Keep_Gripper_Upright_for_Whole_Path:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.enforce_upright_gripper = atoi(sTemp);
        }
        else if(strcmp(sTemp, "Check_Orientation_of_EndEff_at_Goal:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.checkEndEffGoalOrientation = atoi(sTemp);
        }
        else if(strcmp(sTemp,"ARAPlanner_Epsilon:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.epsilon = atof(sTemp);
        }
        else if(strcmp(sTemp,"Length_of_Grasped_Cylinder(meters):") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.grasped_object_length_m = atof(sTemp);
        }
        else if(strcmp(sTemp,"Cylinder_in_Gripper:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.object_grasped = atoi(sTemp);
        }
        else if(strcmp(sTemp,"EndEffGoal_MarginOfError(meters):") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.goal_moe_m = atof(sTemp);
        }
        else if(strcmp(sTemp,"LowRes_Collision_Checking:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.lowres_collision_checking = atoi(sTemp);
        }
        else if(strcmp(sTemp,"MultiRes_Successor_Actions:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.multires_succ_actions = atoi(sTemp);
        }
        else if(strcmp(sTemp,"Increasing_Cell_Costs_Near_Obstacles:") == 0)
        {
            fscanf(fCfg, "%s", sTemp);
            EnvROBARMCfg.variable_cell_costs = atoi(sTemp);
        }
        //parse actions - it must be the last thing in the file
        else if(strcmp(sTemp, "Actions:") == 0)
        {
            break;
        }
        else
        {
            printf("Error: Invalid Field name in parameter file.\n");
            exit(1);
        }
        fscanf(fCfg, "%s", sTemp);
    }

    // parse successor actions
    fscanf(fCfg, "%s", sTemp);
    nrows = atoi(sTemp);
    fscanf(fCfg, "%s", sTemp);
    ncols = atoi(sTemp);

    EnvROBARMCfg.nLowResActions = atoi(sTemp);
    EnvROBARMCfg.nSuccActions = nrows;

    //initialize EnvROBARM.SuccActions & parse config file
    EnvROBARMCfg.SuccActions = new double* [nrows];
    for (x=0; x < nrows; x++)
    {
        EnvROBARMCfg.SuccActions[x] = new double [ncols];
        for(y=0; y < NUMOFLINKS; y++)
        {
            fscanf(fCfg, "%s", sTemp);
            if(!feof(fCfg) && strlen(sTemp) != 0)
                EnvROBARMCfg.SuccActions[x][y] = atoi(sTemp);
            else
            {
                printf("ERROR: End of parameter file reached prematurely. Check for newline.\n");
                exit(1);
            }
        }
    }
}

bool EnvironmentROBARM3D::SetEnvParameter(char* parameter, double value)
{
    if(EnvROBARMCfg.bInitialized == true)
    {
        printf("ERROR: all parameters must be set before initialization of the environment\n");
        return false;
    }

    printf("setting parameter %s to %2.1f\n", parameter, value);

    if(strcmp(parameter, "useDHforFK") == 0)
    {
        EnvROBARMCfg.use_DH = value;
    }
    else if(strcmp(parameter, "enforceMotorLimits") == 0)
    {
        EnvROBARMCfg.enforce_motor_limits = value;
    }
    else if(strcmp(parameter, "useDijkstraHeuristic") == 0)
    {
        EnvROBARMCfg.dijkstra_heuristic = value;
    }
    else if(strcmp(parameter, "paddingSize") == 0)
    {
        EnvROBARMCfg.padding = value;
    }
    else if(strcmp(parameter, "smoothingWeight") == 0)
    {
        EnvROBARMCfg.smoothing_weight = value;
    }
    else if(strcmp(parameter, "usePathSmoothing") == 0)
    {
        EnvROBARMCfg.use_smooth_actions = value;
    }
    else if(strcmp(parameter, "uprightGripperOnly") == 0)
    {
        EnvROBARMCfg.enforce_upright_gripper = value;
    }
    else if(strcmp(parameter, "use6DOFGoal") == 0)
    {
        EnvROBARMCfg.checkEndEffGoalOrientation = value;
    }
    else if(strcmp(parameter,"goalPosMOE") == 0)
    {
        EnvROBARMCfg.goal_moe_m = value;
    }
    else if(strcmp(parameter,"useFastCollisionChecking") == 0)
    {
        EnvROBARMCfg.lowres_collision_checking = value;
    }
    else if(strcmp(parameter,"useMultiResActions") == 0)
    {
        EnvROBARMCfg.multires_succ_actions = value;
    }
    else if(strcmp(parameter,"useHigherCostsNearObstacles") == 0)
    {
        EnvROBARMCfg.variable_cell_costs = value;
    }
    else
    {
        printf("ERROR: invalid parameter %s\n", parameter);
        return false;
    }

    return true;
}

void EnvironmentROBARM3D::InitializeEnvGrid()
{
    int x, y, z;

    // High-Res Grid - allocate the 3D environment & fill set all cells to zero
    EnvROBARMCfg.Grid3D = new char** [EnvROBARMCfg.EnvWidth_c];
    for (x = 0; x < EnvROBARMCfg.EnvWidth_c; x++)
    {
        EnvROBARMCfg.Grid3D[x] = new char* [EnvROBARMCfg.EnvHeight_c];
        for (y = 0; y < EnvROBARMCfg.EnvHeight_c; y++)
        {
            EnvROBARMCfg.Grid3D[x][y] = new char [EnvROBARMCfg.EnvDepth_c];
            for (z = 0; z < EnvROBARMCfg.EnvDepth_c; z++)
                EnvROBARMCfg.Grid3D[x][y][z] = 0;
        }
    }

    //Low-Res Grid - for faster collision checking
    if(EnvROBARMCfg.lowres_collision_checking)
    {
        // Low-Res Grid - allocate the 3D environment & fill set all cells to zero
        EnvROBARMCfg.LowResGrid3D = new char** [EnvROBARMCfg.LowResEnvWidth_c];
        for (x = 0; x < EnvROBARMCfg.LowResEnvWidth_c; x++)
        {
            EnvROBARMCfg.LowResGrid3D[x] = new char* [EnvROBARMCfg.LowResEnvHeight_c];
            for (y = 0; y < EnvROBARMCfg.LowResEnvHeight_c; y++)
            {
                EnvROBARMCfg.LowResGrid3D[x][y] = new char [EnvROBARMCfg.LowResEnvDepth_c];
                for (z = 0; z < EnvROBARMCfg.LowResEnvDepth_c; z++)
                    EnvROBARMCfg.LowResGrid3D[x][y][z] = 0;
            }
        }
        printf("Allocated LowResGrid3D.\n");
    }
}

void EnvironmentROBARM3D::DiscretizeAngles()
{
    int i;
//     double HalfGridCell = EnvROBARMCfg.GridCellWidth/2.0;
    for(i = 0; i < NUMOFLINKS; i++)
    {
        EnvROBARMCfg.angledelta[i] = (2.0*PI_CONST) / ANGLEDELTA; 
        EnvROBARMCfg.anglevals[i] = ANGLEDELTA;

//         if (EnvROBARMCfg.LinkLength_m[i] > 0)
//             EnvROBARMCfg.angledelta[i] =  2*asin(HalfGridCell/EnvROBARMCfg.LinkLength_m[i]);
//         else
//             EnvROBARMCfg.angledelta[i] = ANGLEDELTA;

//         EnvROBARMCfg.anglevals[i] = (int)(2.0*PI_CONST/EnvROBARMCfg.angledelta[i]+0.99999999);
    }
}

//angles are counterclockwise from 0 to 360 in radians, 0 is the center of bin 0, ...
void EnvironmentROBARM3D::ComputeContAngles(short unsigned int coord[NUMOFLINKS], double angle[NUMOFLINKS])
{
    int i;
    for(i = 0; i < NUMOFLINKS; i++)
    {
        angle[i] = coord[i]*EnvROBARMCfg.angledelta[i];
    }
}

void EnvironmentROBARM3D::ComputeCoord(double angle[NUMOFLINKS], short unsigned int coord[NUMOFLINKS])
{
    int i;
    for(i = 0; i < NUMOFLINKS; i++)
    {
        coord[i] = (int)((angle[i] + EnvROBARMCfg.angledelta[i]*0.5)/EnvROBARMCfg.angledelta[i]);
        if(coord[i] == EnvROBARMCfg.anglevals[i])
                coord[i] = 0;
    }
}

// convert a cell in the occupancy grid to point in real world 
void EnvironmentROBARM3D::Cell2ContXYZ(int x, int y, int z, double *pX, double *pY, double *pZ)
{
    // offset the arm in the map so that it is placed in the middle of the world 
    int yoffset_c = (EnvROBARMCfg.EnvWidth_c-1) / 2;

    *pX = (x /*- X_OFFSET_C*/) * EnvROBARMCfg.GridCellWidth + EnvROBARMCfg.GridCellWidth*0.5;
    *pY = (y - yoffset_c/*Y_OFFSET_C*/) * EnvROBARMCfg.GridCellWidth + EnvROBARMCfg.GridCellWidth*0.5;
    *pZ = (z /*- Z_OFFSET_C*/) *EnvROBARMCfg.GridCellWidth + EnvROBARMCfg.GridCellWidth*0.5;
}

// convert a point in real world to a cell in occupancy grid 
void EnvironmentROBARM3D::ContXYZ2Cell(double x, double y, double z, short unsigned int *pX, short unsigned int *pY, short unsigned int *pZ)
{
    // offset the arm in the map so that it is placed in the middle of the world 
    int yoffset_c = (EnvROBARMCfg.EnvHeight_c-1) / 2;

    //take the nearest cell
    *pX = (int)((x/EnvROBARMCfg.GridCellWidth));
    if(x < 0) *pX = 0;
    if(*pX >= EnvROBARMCfg.EnvWidth_c) *pX = EnvROBARMCfg.EnvWidth_c-1;

    *pY = (int)((y/EnvROBARMCfg.GridCellWidth) + yoffset_c);
    if(y + (yoffset_c*EnvROBARMCfg.GridCellWidth) < 0) *pY = 0;
    if(*pY >= EnvROBARMCfg.EnvHeight_c) *pY = EnvROBARMCfg.EnvHeight_c-1;

    *pZ = (int)((z/EnvROBARMCfg.GridCellWidth));
    if(z < 0) *pZ = 0;
    if(*pZ >= EnvROBARMCfg.EnvDepth_c) *pZ = EnvROBARMCfg.EnvDepth_c-1;
}

void EnvironmentROBARM3D::ContXYZ2Cell(double* xyz, double gridcellwidth, int dims_c[3], short unsigned int *pXYZ)
{
    // offset the arm in the map so that it is placed in the middle of the world 
    int yoffset_c = (dims_c[1]-1) / 2;

    //take the nearest cell
    pXYZ[0] = (int)(xyz[0]/gridcellwidth);
    if(xyz[0] < 0)
        pXYZ[0] = 0;
    if(pXYZ[0] >= dims_c[0])
        pXYZ[0] = dims_c[0]-1;

    pXYZ[1] = (int)((xyz[1]/gridcellwidth) + yoffset_c);
    if(xyz[1] + (yoffset_c * gridcellwidth) < 0)
         pXYZ[1] = 0;
    if(pXYZ[1] >= dims_c[1])
        pXYZ[1] = dims_c[1]-1;

    pXYZ[2] = (int)(xyz[2]/gridcellwidth);
    if(xyz[2] < 0) 
        pXYZ[2] = 0;
    if(pXYZ[2] >= dims_c[2]) 
        pXYZ[2] = dims_c[2]-1;
}

// convert a point in real world to a cell in occupancy grid
void EnvironmentROBARM3D::ContXYZ2Cell(double x, double y, double z, int *pX, int *pY, int *pZ)
{
    // offset the arm in the map so that it is placed in the middle of the world 
    int yoffset_c = (EnvROBARMCfg.EnvWidth_c-1) / 2;

    //take the nearest cell
    *pX = (int)((x/EnvROBARMCfg.GridCellWidth));// + X_OFFSET_C);
    if( x < 0) *pX = 0;
    if( *pX >= EnvROBARMCfg.EnvWidth_c) *pX = EnvROBARMCfg.EnvWidth_c-1;

    *pY = (int)((y/EnvROBARMCfg.GridCellWidth) + yoffset_c);//Y_OFFSET_C);
    if( y + (yoffset_c*EnvROBARMCfg.GridCellWidth) < 0) *pY = 0;
    if( *pY >= EnvROBARMCfg.EnvHeight_c) *pY = EnvROBARMCfg.EnvHeight_c-1;

    *pZ = (int)((z/EnvROBARMCfg.GridCellWidth));// + Z_OFFSET_C);
    if( z < 0) *pZ = 0;
    if( *pZ >= EnvROBARMCfg.EnvDepth_c) *pZ = EnvROBARMCfg.EnvDepth_c-1;
}

/*//add obstacles to grid (right now just supports cubes)

void EnvironmentROBARM3D::AddObstacleToGrid(double* obstacle, int type, char*** grid, double gridcell_m)
{
    int x, y, z, pX_max, pX_min, pY_max, pY_min, pZ_max, pZ_min;
    int padding_c = EnvROBARMCfg.padding*2 / gridcell_m + 0.5;
    short unsigned int obstacle_c[6] = {0};
    int dims_c[3] = {EnvROBARMCfg.EnvWidth_c, EnvROBARMCfg.EnvHeight_c, EnvROBARMCfg.EnvDepth_c};

    if(gridcell_m == EnvROBARMCfg.LowResGridCellWidth)
    {
        dims_c[0] = EnvROBARMCfg.LowResEnvWidth_c;
        dims_c[1] = EnvROBARMCfg.LowResEnvHeight_c;
        dims_c[2] = EnvROBARMCfg.LowResEnvDepth_c;
    }

    //cube(meters)
    if (type == 0)
    {
        double obs[3] = {obstacle[0],obstacle[1],obstacle[2]};
        short unsigned int obs_c[3] = {obstacle_c[0],obstacle_c[1],obstacle_c[2]};

        ContXYZ2Cell(obs, gridcell_m, dims_c, obs_c);
        //ContXYZ2Cell(obstacle[0],obstacle[1],obstacle[2],&(obstacle_c[0]),&(obstacle_c[1]),&(obstacle_c[2]));

        //get dimensions (width,height,depth)
        obstacle_c[3] = abs(obstacle[3]+EnvROBARMCfg.padding*2) / gridcell_m + 0.5;
        obstacle_c[4] = abs(obstacle[4]+EnvROBARMCfg.padding*2) / gridcell_m + 0.5;
        obstacle_c[5] = abs(obstacle[5]+ EnvROBARMCfg.padding*2) / gridcell_m + 0.5;

        pX_max = obs_c[0] + obstacle_c[3]/2;
        pX_min = obs_c[0] - obstacle_c[3]/2;
        pY_max = obs_c[1] + obstacle_c[4]/2;
        pY_min = obs_c[1] - obstacle_c[4]/2;
        pZ_max = obs_c[2] + obstacle_c[5]/2;
        pZ_min = obs_c[2] - obstacle_c[5]/2;
    }
    //cube(cells)
    else if(type == 1)
    {
        //convert to short unsigned int from double
        obstacle_c[0] = obstacle[0];
        obstacle_c[1] = obstacle[1];
        obstacle_c[2] = obstacle[2];
        obstacle_c[3] = abs(obstacle[3] + padding_c);
        obstacle_c[4] = abs(obstacle[4] + padding_c);
        obstacle_c[5] = abs(obstacle[5] + padding_c);

        pX_max = obstacle_c[0] + obstacle_c[3]/2;
        pX_min = obstacle_c[0] - obstacle_c[3]/2;
        pY_max = obstacle_c[1] + obstacle_c[4]/2;
        pY_min = obstacle_c[1] - obstacle_c[4]/2;
        pZ_max = obstacle_c[2] + obstacle_c[5]/2;
        pZ_min = obstacle_c[2] - obstacle_c[5]/2;
    }
    else
    {
        printf("Error: Attempted to add undefined type of obstacle to grid.\n");
        return; 
    }

    // bounds checking, cutoff object if out of bounds
    if(pX_max > dims_c[0] - 1)
        pX_max = dims_c[0] - 1;
    if (pX_min < 0)
        pX_min = 0;
    if(pY_max > dims_c[1] - 1)
        pY_max = dims_c[1] - 1;
    if (pY_min < 0)
        pY_min = 0;
    if(pZ_max > dims_c[2] - 1)
        pZ_max = dims_c[2] - 1;
    if (pZ_min < 0)
        pZ_min = 0;

    // assign the cells occupying the obstacle to 1
    int b = 0;
    for (y = pY_min; y <= pY_max; y++)
    {
        for (x = pX_min; x <= pX_max; x++)
        {
            for (z = pZ_min; z <= pZ_max; z++)
            {
                grid[x][y][z] = 1;
                b++;
            }
        }
    }
//     printf("%i %i %i %i %i %i\n", obstacle_c[0],obstacle_c[1],obstacle_c[2],obstacle_c[3],obstacle_c[4],obstacle_c[5]);
//     printf("Obstacle %i cells\n",b);
}
*/

void EnvironmentROBARM3D::AddObstacleToGrid(double* obstacle, int type, char*** grid, double gridcell_m)
{
    int x, y, z, pX_max, pX_min, pY_max, pY_min, pZ_max, pZ_min;
    int padding_c = EnvROBARMCfg.padding*2 / gridcell_m + 0.5;
    short unsigned int obstacle_c[6] = {0};
    int dims_c[3] = {EnvROBARMCfg.EnvWidth_c, EnvROBARMCfg.EnvHeight_c, EnvROBARMCfg.EnvDepth_c};

    if(gridcell_m == EnvROBARMCfg.LowResGridCellWidth)
    {
        dims_c[0] = EnvROBARMCfg.LowResEnvWidth_c;
        dims_c[1] = EnvROBARMCfg.LowResEnvHeight_c;
        dims_c[2] = EnvROBARMCfg.LowResEnvDepth_c;
    }

    //cube(meters)
    if (type == 0)
    {
        double obs[3] = {obstacle[0],obstacle[1],obstacle[2]};
        short unsigned int obs_c[3];// = {obstacle_c[0], obstacle_c[1], obstacle_c[2]};

        ContXYZ2Cell(obs, gridcell_m, dims_c, obs_c);

        //get dimensions of obstacles in cells (width,height,depth)
        obstacle_c[3] = obstacle[3] / gridcell_m + 0.5;
        obstacle_c[4] = obstacle[4] / gridcell_m + 0.5;
        obstacle_c[5] = obstacle[5] / gridcell_m + 0.5;

        pX_max = obs_c[0] + obstacle_c[3]/2 + padding_c;
        pX_min = obs_c[0] - obstacle_c[3]/2 - padding_c;
        pY_max = obs_c[1] + obstacle_c[4]/2 + padding_c;
        pY_min = obs_c[1] - obstacle_c[4]/2 - padding_c;
        pZ_max = obs_c[2] + obstacle_c[5]/2 + padding_c;
        pZ_min = obs_c[2] - obstacle_c[5]/2 - padding_c;
    }

    //cube(cells)
    else if(type == 1)
    {
        //convert to short unsigned int from double
        obstacle_c[0] = obstacle[0];
        obstacle_c[1] = obstacle[1];
        obstacle_c[2] = obstacle[2];
        obstacle_c[3] = abs(obstacle[3] + padding_c);
        obstacle_c[4] = abs(obstacle[4] + padding_c);
        obstacle_c[5] = abs(obstacle[5] + padding_c);

        pX_max = obstacle_c[0] + obstacle_c[3]/2;
        pX_min = obstacle_c[0] - obstacle_c[3]/2;
        pY_max = obstacle_c[1] + obstacle_c[4]/2;
        pY_min = obstacle_c[1] - obstacle_c[4]/2;
        pZ_max = obstacle_c[2] + obstacle_c[5]/2;
        pZ_min = obstacle_c[2] - obstacle_c[5]/2;
    }
    else
    {
        printf("Error: Attempted to add undefined type of obstacle to grid.\n");
        return; 
    }

    // bounds checking, cutoff object if out of bounds
    if(pX_max > dims_c[0] - 1)
        pX_max = dims_c[0] - 1;
    if (pX_min < 0)
        pX_min = 0;
    if(pY_max > dims_c[1] - 1)
        pY_max = dims_c[1] - 1;
    if (pY_min < 0)
        pY_min = 0;
    if(pZ_max > dims_c[2] - 1)
        pZ_max = dims_c[2] - 1;
    if (pZ_min < 0)
        pZ_min = 0;

    // assign the cells occupying the obstacle to ObstacleCost
    int b = 0;
    for (y = pY_min; y <= pY_max; y++)
    {
        for (x = pX_min; x <= pX_max; x++)
        {
            for (z = pZ_min; z <= pZ_max; z++)
            {
                grid[x][y][z] = EnvROBARMCfg.ObstacleCost;
                b++;
            }
        }
    }

    if(EnvROBARMCfg.variable_cell_costs)
    {
        //apply cost to cells close to obstacles
        pX_min -= EnvROBARMCfg.medCostRadius_c;
        pX_max += EnvROBARMCfg.medCostRadius_c;
        pY_min -= EnvROBARMCfg.medCostRadius_c;
        pY_max += EnvROBARMCfg.medCostRadius_c;
        pZ_min -= EnvROBARMCfg.medCostRadius_c;
        pZ_max += EnvROBARMCfg.medCostRadius_c;

        // bounds checking, cutoff object if out of bounds
        if(pX_max > dims_c[0] - 1)
            pX_max = dims_c[0] - 1;
        if (pX_min < 0)
            pX_min = 0;
        if(pY_max > dims_c[1] - 1)
            pY_max = dims_c[1] - 1;
        if (pY_min < 0)
            pY_min = 0;
        if(pZ_max > dims_c[2] - 1)
            pZ_max = dims_c[2] - 1;
        if (pZ_min < 0)
            pZ_min = 0;

        // assign the cells close to the obstacle to medObstacleCost
        b = 0;
        for (y = pY_min; y <= pY_max; y++)
        {
            for (x = pX_min; x <= pX_max; x++)
            {
                for (z = pZ_min; z <= pZ_max; z++)
                {
                    if(grid[x][y][z] < EnvROBARMCfg.medObstacleCost)
                    {
                        grid[x][y][z] = EnvROBARMCfg.medObstacleCost;
                        b++;
                    }
                }
            }
        }

        //apply cost to cells less close to obstacles
        pX_min -= EnvROBARMCfg.lowCostRadius_c;
        pX_max += EnvROBARMCfg.lowCostRadius_c;
        pY_min -= EnvROBARMCfg.lowCostRadius_c;
        pY_max += EnvROBARMCfg.lowCostRadius_c;
        pZ_min -= EnvROBARMCfg.lowCostRadius_c;
        pZ_max += EnvROBARMCfg.lowCostRadius_c;

        // bounds checking, cutoff object if out of bounds
        if(pX_max > dims_c[0] - 1)
            pX_max = dims_c[0] - 1;
        if (pX_min < 0)
            pX_min = 0;
        if(pY_max > dims_c[1] - 1)
            pY_max = dims_c[1] - 1;
        if (pY_min < 0)
            pY_min = 0;
        if(pZ_max > dims_c[2] - 1)
            pZ_max = dims_c[2] - 1;
        if (pZ_min < 0)
            pZ_min = 0;

        // assign the cells close to the obstacle to lowObstacleCost
        b = 0;
        for (y = pY_min; y <= pY_max; y++)
        {
            for (x = pX_min; x <= pX_max; x++)
            {
                for (z = pZ_min; z <= pZ_max; z++)
                {
                    if(grid[x][y][z] < EnvROBARMCfg.lowObstacleCost)
                    {
                        grid[x][y][z] = EnvROBARMCfg.lowObstacleCost;
                        b++;
                    }
                }
            }
        }
    }
}

//returns 1 if end effector within space, 0 otherwise
int EnvironmentROBARM3D::ComputeEndEffectorPos(double angles[NUMOFLINKS], short unsigned int endeff[3], short unsigned int wrist[3], short unsigned int elbow[3], double orientation[3][3])
{
    num_forwardkinematics++;
    clock_t currenttime = clock();
    double x,y,z;
    int retval = 1;

    //convert angles from positive values in radians (from 0->6.28) to centered around 0 (not really needed)
    for (int i = 0; i < NUMOFLINKS; i++)
    {
        if(angles[i] >= PI_CONST)
            angles[i] = -2.0*PI_CONST + angles[i];
    }

    //use DH or Kinematics Library for forward kinematics
    if(EnvROBARMCfg.use_DH)
    {
        ComputeForwardKinematics_DH(angles);

        //get position of elbow
        x = EnvROBARM.Trans[0][3][3] + EnvROBARMCfg.BaseX_m;
        y = EnvROBARM.Trans[1][3][3] + EnvROBARMCfg.BaseY_m;
        z = EnvROBARM.Trans[2][3][3] + EnvROBARMCfg.BaseZ_m;
        ContXYZ2Cell(x, y, z, &(elbow[0]), &(elbow[1]), &(elbow[2]));

        //get position of wrist
        x = EnvROBARM.Trans[0][3][5] + EnvROBARMCfg.BaseX_m;
        y = EnvROBARM.Trans[1][3][5] + EnvROBARMCfg.BaseY_m;
        z = EnvROBARM.Trans[2][3][5] + EnvROBARMCfg.BaseZ_m;
        ContXYZ2Cell(x, y, z, &(wrist[0]), &(wrist[1]), &(wrist[2]));

        //get position of tip of gripper
        x = EnvROBARM.Trans[0][3][7] + EnvROBARMCfg.BaseX_m;
        y = EnvROBARM.Trans[1][3][7] + EnvROBARMCfg.BaseY_m;
        z = EnvROBARM.Trans[2][3][7] + EnvROBARMCfg.BaseZ_m;
        ContXYZ2Cell(x, y, z, &(endeff[0]),&(endeff[1]),&(endeff[2]));

        // if end effector is out of bounds then return 0
        if(endeff[0] >= EnvROBARMCfg.EnvWidth_c || endeff[1] >= EnvROBARMCfg.EnvHeight_c || endeff[2] >= EnvROBARMCfg.EnvDepth_c)
            retval =  0;

        DH_time += clock() - currenttime;
    }
    else    //use Kinematics Library
    {
        //get position of elbow
        ComputeForwardKinematics_ROS(angles, 4, &x, &y, &z);
        ContXYZ2Cell(x, y, z, &(elbow[0]), &(elbow[1]), &(elbow[2]));

        //get position of wrist
        ComputeForwardKinematics_ROS(angles, 6, &x, &y, &z);
        ContXYZ2Cell(x, y, z, &(wrist[0]), &(wrist[1]), &(wrist[2]));

        //get position of tip of gripper
        ComputeForwardKinematics_ROS(angles, 7, &x, &y, &z);
        ContXYZ2Cell(x, y, z, &(endeff[0]), &(endeff[1]), &(endeff[2]));

        // check upper bounds
        if(endeff[0] >= EnvROBARMCfg.EnvWidth_c || endeff[1] >= EnvROBARMCfg.EnvHeight_c || endeff[2] >= EnvROBARMCfg.EnvDepth_c)
            retval =  0;

        KL_time += clock() - currenttime;
    }

    // check if orientation of gripper is upright (for the whole path)
    if(EnvROBARMCfg.enforce_upright_gripper)
    {
        if (EnvROBARM.Trans[2][0][7] < 1.0 - EnvROBARMCfg.gripper_orientation_moe)
            retval = 0;
    }

//     double Rot[3][3];

    //store the orientation of gripper
//     if(EnvROBARMCfg.enforce_upright_gripper ||  EnvROBARMCfg.checkEndEffGoalOrientation)
//     {
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                orientation[i][j] = EnvROBARM.Trans[i][j][7];
//                 Rot[i][j] = EnvROBARM.Trans[i][j][7];
            }
        }
//     }

//     double roll, pitch, yaw;
//     getRPY(Rot, &roll, &pitch, &yaw);
// 
//     printf("{Rot:  ");
//     for (int u = 0; u < 3; u++)
//     {
//         for(int p = 0; p < 3; p++)
//             printf("%2.2f  ",Rot[u][p]);
//     }
//     printf("Roll:  %3.2f    Pitch:  %3.2f   Yaw:   %3.2f\n", roll, pitch, yaw);

    return retval;
}

//returns end effector position in meters
int EnvironmentROBARM3D::ComputeEndEffectorPos(double angles[NUMOFLINKS], double endeff_m[3])
{
    int retval = 1;

    //convert angles from positive values in radians (from 0->6.28) to centered around 0
    for (int i = 0; i < NUMOFLINKS; i++)
    {
        if(angles[i] >= PI_CONST)
            angles[i] = -2.0*PI_CONST + angles[i];
    }

    //use DH or Kinematics Library for forward kinematics
    if(EnvROBARMCfg.use_DH)
    {
        ComputeForwardKinematics_DH(angles);

        //get position of tip of gripper
        endeff_m[0] = EnvROBARM.Trans[0][3][7] + EnvROBARMCfg.BaseX_m;
        endeff_m[1] = EnvROBARM.Trans[1][3][7] + EnvROBARMCfg.BaseY_m;
        endeff_m[2] = EnvROBARM.Trans[2][3][7] + EnvROBARMCfg.BaseZ_m;
    }
    else
        ComputeForwardKinematics_ROS(angles, 7, &(endeff_m[0]), &(endeff_m[1]), &(endeff_m[2]));

    return retval;
}

//returns end effector position in cells
int EnvironmentROBARM3D::ComputeEndEffectorPos(double angles[NUMOFLINKS], short unsigned int endeff[3])
{
    int retval = 1;
    double endeff_m[3];

    //convert angles from positive values in radians (from 0->6.28) to centered around 0
    for (int i = 0; i < NUMOFLINKS; i++)
    {
        if(angles[i] >= PI_CONST)
            angles[i] = -2.0*PI_CONST + angles[i];
    }

    //use DH or Kinematics Library for forward kinematics
    if(EnvROBARMCfg.use_DH)
    {
        ComputeForwardKinematics_DH(angles);

        //get position of tip of gripper
        endeff_m[0] = EnvROBARM.Trans[0][3][7] + EnvROBARMCfg.BaseX_m;
        endeff_m[1] = EnvROBARM.Trans[1][3][7] + EnvROBARMCfg.BaseY_m;
        endeff_m[2] = EnvROBARM.Trans[2][3][7] + EnvROBARMCfg.BaseZ_m;
    }
    else
        ComputeForwardKinematics_ROS(angles, 7, &(endeff_m[0]), &(endeff_m[1]), &(endeff_m[2]));

    ContXYZ2Cell(endeff_m[0],endeff_m[1],endeff_m[2], &(endeff[0]),&(endeff[1]),&(endeff[2]));
    return retval;
}

//if pTestedCells is NULL, then the tested points are not saved and it is more
//efficient as it returns as soon as it sees first invalid point
int EnvironmentROBARM3D::IsValidLineSegment(double x0, double y0, double z0, double x1, double y1, double z1, char*** Grid3D, vector<CELLV>* pTestedCells)
{
    bresenham_param_t params;
    int nX, nY, nZ; 
    short unsigned int nX0, nY0, nZ0, nX1, nY1, nZ1;
    int retvalue = 1;
    CELLV tempcell;

    //make sure the line segment is inside the environment
    if(x0 < 0 || x0 >= EnvROBARMCfg.EnvWidth_m || x1 < 0 || x1 >= EnvROBARMCfg.EnvWidth_m ||
        y0 < 0 || y0 >= EnvROBARMCfg.EnvHeight_m || y1 < 0 || y1 >= EnvROBARMCfg.EnvHeight_m ||
        z0 < 0 || z0 >= EnvROBARMCfg.EnvDepth_m || z1 < 0 || z1 >= EnvROBARMCfg.EnvDepth_m)
    {
        return 0;
    }

    ContXYZ2Cell(x0, y0, z0, &nX0, &nY0, &nZ0);
    ContXYZ2Cell(x1, y1, z1, &nX1, &nY1, &nZ1);

    //iterate through the points on the segment
    get_bresenham_parameters3d(nX0, nY0, nZ0, nX1, nY1, nZ1, &params);
    do {
            get_current_point3d(&params, &nX, &nY, &nZ);
            if(Grid3D[nX][nY][nZ] >= EnvROBARMCfg.ObstacleCost)
            {
                if(pTestedCells == NULL)
                    return 0;
                else
                    retvalue = 0;
            }

            //insert the tested point
            if(pTestedCells)
            {
                tempcell.bIsObstacle = (Grid3D[nX][nY][nZ] >= EnvROBARMCfg.ObstacleCost);
                tempcell.x = nX;
                tempcell.y = nY;
                tempcell.z = nZ;
                pTestedCells->push_back(tempcell);
            }
    } while (get_next_point3d(&params));

    return retvalue;
}

int EnvironmentROBARM3D::IsValidLineSegment(short unsigned int x0, short unsigned int y0, short unsigned int z0, short unsigned int x1, short unsigned int y1, short unsigned int z1, char*** Grid3D, vector<CELLV>* pTestedCells)
{
    bresenham_param_t params;
    int nX, nY, nZ; 
    int retvalue = 1;
    CELLV tempcell;

    short unsigned int width = EnvROBARMCfg.EnvWidth_c;
    short unsigned int height = EnvROBARMCfg.EnvHeight_c;
    short unsigned int depth = EnvROBARMCfg.EnvDepth_c;

    //use low resolution grid if enabled
    if(EnvROBARMCfg.lowres_collision_checking)
    {
        width = EnvROBARMCfg.LowResEnvWidth_c;
        height = EnvROBARMCfg.LowResEnvHeight_c;
        depth = EnvROBARMCfg.LowResEnvDepth_c;
    }

    //make sure the line segment is inside the environment - no need to check < 0
    if(x0 >= width || x1 >= width ||
       y0 >= height || y1 >= height ||
       z0 >= depth || z1 >= depth)
    {
        return 0;
    }

    //iterate through the points on the segment
    get_bresenham_parameters3d(x0, y0, z0, x1, y1, z1, &params);
    do {
        get_current_point3d(&params, &nX, &nY, &nZ);
        if(Grid3D[nX][nY][nZ] >= EnvROBARMCfg.ObstacleCost)
        {
            if(pTestedCells == NULL)
                return 0;
            else
                retvalue = 0;
        }

        //insert the tested point
        if(pTestedCells)
        {
            tempcell.bIsObstacle = (Grid3D[nX][nY][nZ] >= EnvROBARMCfg.ObstacleCost);
            tempcell.x = nX;
            tempcell.y = nY;
            tempcell.z = nZ;
            pTestedCells->push_back(tempcell);
        }
    } while (get_next_point3d(&params));

    return retvalue;
}

int EnvironmentROBARM3D::IsValidCoord(short unsigned int coord[NUMOFLINKS], short unsigned int endeff_pos[3], short unsigned int wrist_pos[3], short unsigned int elbow_pos[3], double orientation[3][3])
{
    //for stats
    clock_t currenttime = clock();

    int grid_dims[3] = {EnvROBARMCfg.EnvWidth_c, EnvROBARMCfg.EnvHeight_c, EnvROBARMCfg.EnvDepth_c};
    short unsigned int endeff[3] = {endeff_pos[0],endeff_pos[1],endeff_pos[2]};
    short unsigned int wrist[3] = {wrist_pos[0],wrist_pos[1],wrist_pos[2]};
    short unsigned int elbow[3] = {elbow_pos[0], elbow_pos[1], elbow_pos[2]};
    short unsigned int shoulder[3] = {EnvROBARMCfg.BaseX_c, EnvROBARMCfg.BaseY_c, EnvROBARMCfg.BaseZ_c};

    double fingertips_g[3] = {0, 0, GRIPPER_LENGTH_M / EnvROBARMCfg.GridCellWidth};
    short unsigned int fingertips_s[3] = {0};

    char*** Grid3D = EnvROBARMCfg.Grid3D;

    double angles[NUMOFLINKS], angles_0[NUMOFLINKS];
    int retvalue = 1;
    vector<CELLV>* pTestedCells = NULL;
    ComputeContAngles(coord, angles);

    //for low resolution collision checking
    if(EnvROBARMCfg.lowres_collision_checking)
    {
        grid_dims[0] = EnvROBARMCfg.LowResEnvWidth_c;
        grid_dims[1] = EnvROBARMCfg.LowResEnvHeight_c;
        grid_dims[2] = EnvROBARMCfg.LowResEnvDepth_c;
        Grid3D = EnvROBARMCfg.LowResGrid3D;

        HighResGrid2LowResGrid(endeff, endeff);
        HighResGrid2LowResGrid(wrist, wrist);
        HighResGrid2LowResGrid(elbow, elbow);
        HighResGrid2LowResGrid(shoulder, shoulder);

        fingertips_g[2] = GRIPPER_LENGTH_M / EnvROBARMCfg.LowResGridCellWidth;
    }

    // check motor limits
    if(EnvROBARMCfg.enforce_motor_limits)
    {
        //convert angles from positive values in radians (from 0->6.28) to centered around 0
        for (int i = 0; i < NUMOFLINKS; i++)
        {
            angles_0[i] = angles[i];
            if(angles[i] >= PI_CONST)
                angles_0[i] = -2.0*PI_CONST + angles[i];
        }

        //shoulder pan - Left is Positive Direction
        if (angles_0[0] > EnvROBARMCfg.PosMotorLimits[0] || angles_0[0] < EnvROBARMCfg.NegMotorLimits[0])
            return 0;

        //shoulder pitch - Down is Positive Direction
        if (angles_0[1] > EnvROBARMCfg.PosMotorLimits[1] || angles_0[1] < EnvROBARMCfg.NegMotorLimits[1])
           return 0;

        //upperarm roll
        if (angles_0[2] > EnvROBARMCfg.PosMotorLimits[2] || angles_0[2] < EnvROBARMCfg.NegMotorLimits[2])
           return 0;

        //elbow flex - Down is Positive Direction
        if (angles_0[3] > EnvROBARMCfg.PosMotorLimits[3] || angles_0[3] < EnvROBARMCfg.NegMotorLimits[3])
            return 0;

        //forearm roll
        if (angles_0[4] > EnvROBARMCfg.PosMotorLimits[4] || angles_0[4] < EnvROBARMCfg.NegMotorLimits[4])
            return 0;

        //wrist flex - Down is Positive Direction
        if (angles_0[5] > EnvROBARMCfg.PosMotorLimits[5] || angles_0[5] < EnvROBARMCfg.NegMotorLimits[5])
            return 0;

        //wrist roll
        if (angles_0[6] > EnvROBARMCfg.PosMotorLimits[6] || angles_0[6] < EnvROBARMCfg.NegMotorLimits[6])
            return 0;
    }

    // check if only end effector position is valid
    if (EnvROBARMCfg.endeff_check_only)
    {
        //bounds checking on upper bound (short unsigned int cannot be less than 0, so just check maxes)
        if(endeff[0] >= grid_dims[0] || endeff[1] >= grid_dims[1] || endeff[2] >= grid_dims[2])   
        {
            check_collision_time += clock() - currenttime;
            return 0;
        }

        if(pTestedCells)
        {
            CELLV tempcell;
            tempcell.bIsObstacle = Grid3D[endeff[0]][endeff[1]][endeff[2]];
            tempcell.x = endeff[0];
            tempcell.y = endeff[1];
            tempcell.z = endeff[2];

            pTestedCells->push_back(tempcell);
        }

        //check end effector is not hitting obstacle
        if(Grid3D[endeff[0]][endeff[1]][endeff[2]] >= EnvROBARMCfg.ObstacleCost)
        {
            check_collision_time += clock() - currenttime;
            return 0;
        }
    }

    // check if full arm with object in gripper are valid as well
    else 
    {
        //bounds checking on upper bound of map - should really only check end effector not other joints
        if(endeff[0] >= grid_dims[0] || endeff[1] >= grid_dims[1] || endeff[2] >= grid_dims[2] ||
           wrist[0] >= grid_dims[0] || wrist[1] >= grid_dims[1] || wrist[2] >= grid_dims[2] ||
           elbow[0] >= grid_dims[0] || elbow[1] >= grid_dims[1] || elbow[2] >= grid_dims[2])
        {
//             printf("[IsValidCoord] Arm joint bounds checking failed.\n");
            check_collision_time += clock() - currenttime;
            return 0;
        }

        //check if joints are hitting obstacle - is not needed because IsValidLineSegment checks it
        if(Grid3D[endeff[0]][endeff[1]][endeff[2]] >= EnvROBARMCfg.ObstacleCost ||
           Grid3D[wrist[0]][wrist[1]][wrist[2]] >= EnvROBARMCfg.ObstacleCost ||
           Grid3D[elbow[0]][elbow[1]][elbow[2]] >= EnvROBARMCfg.ObstacleCost)
        {
//             printf("[IsValidCoord] Arm joint collision checking failed.\n");
            check_collision_time += clock() - currenttime;
            return 0;
        }

        //check the validity of the corresponding arm links (line segments)
        if(!IsValidLineSegment(shoulder[0],shoulder[1],shoulder[2], elbow[0],elbow[1],elbow[2], Grid3D, pTestedCells) ||
            !IsValidLineSegment(elbow[0],elbow[1],elbow[2],wrist[0],wrist[1],wrist[2], Grid3D, pTestedCells) ||
            !IsValidLineSegment(wrist[0],wrist[1],wrist[2],endeff[0],endeff[1], endeff[2], Grid3D, pTestedCells))
        {
            if(pTestedCells == NULL)
            {
//                 printf("[IsValidCoord] shoulder: (%i %i %i)\n",shoulder[0],shoulder[1],shoulder[2]);
//                 printf("[IsValidCoord] elbow: (%i %i %i)\n",elbow[0],elbow[1],elbow[2]);
//                 printf("[IsValidCoord] wrist: (%i %i %i)\n",wrist[0],wrist[1],wrist[2]);
//                 printf("[IsValidCoord] endeff: (%i %i %i)\n",endeff[0],endeff[1],endeff[2]);
//                 printf("[IsValidCoord] Arm link collision checking failed.\n");
                check_collision_time += clock() - currenttime;
                return 0;
            }
            else
            {
//                 printf("[IsValidCoord] Arm link collision checking failed..\n");
                retvalue = 0;
            }
        }

        //check if gripper is clear of obstacles
        if(EnvROBARMCfg.use_DH)
        {
            //get position of fingertips in shoulder frame (assuming gripper is closed)
            //P_objectInshoulder = R_gripperToshoulder * P_objectIngripper + P_gripperInshoulder
            fingertips_s[0] = (orientation[0][0]*fingertips_g[0] + orientation[0][1]*fingertips_g[1] + orientation[0][2]*fingertips_g[2]) + endeff[0];
            fingertips_s[1] = (orientation[1][0]*fingertips_g[0] + orientation[1][1]*fingertips_g[1] + orientation[1][2]*fingertips_g[2]) + endeff[1];
            fingertips_s[2] = (orientation[2][0]*fingertips_g[0] + orientation[2][1]*fingertips_g[1] + orientation[2][2]*fingertips_g[2]) + endeff[2];

            pTestedCells = NULL;
            if(!IsValidLineSegment(endeff[0],endeff[1],endeff[2],fingertips_s[0], fingertips_s[1],fingertips_s[2], Grid3D, pTestedCells))
            {
                if(pTestedCells == NULL)
                {
                    check_collision_time += clock() - currenttime;
                    return 0;
                }
                else
                    retvalue = 0;
            }
        }

//         printf("[IsValidCoord] elbow: (%u,%u,%u)  wrist:(%u,%u,%u) endeff:(%u,%u,%u) fingertips: (%u %u %u)\n", elbow[0],elbow[1],elbow[2],
//                     wrist[0], wrist[1], wrist[2], endeff[0], endeff[1], endeff[2], fingertips_s[0], fingertips_s[1], fingertips_s[2]);

        //check line segment of object in gripper for collision
        if(EnvROBARMCfg.object_grasped)
        {
            double objectAbove_g[3] = {0}, objectBelow_g[3] = {0};
            short unsigned int objectAbove_s[3], objectBelow_s[3]; //these types are wrong

                //the object is along the gripper's x-axis
            objectBelow_g[0] = -EnvROBARMCfg.grasped_object_length_m / EnvROBARMCfg.GridCellWidth;

            //for now the gripper is gripping the top of the cylindrical object 
            //which means, the end effector is the bottom point of the object

            //get position of one end of object in shoulder frame
            //P_objectInshoulder = R_gripperToshoulder * P_objectIngripper + P_gripperInshoulder
            objectAbove_s[0] = (orientation[0][0]*objectAbove_g[0] + orientation[0][1]*objectAbove_g[1] + orientation[0][2]* objectAbove_g[2]) + endeff[0];
            objectAbove_s[1] = (orientation[1][0]*objectAbove_g[0] + orientation[1][1]*objectAbove_g[1] + orientation[1][2]* objectAbove_g[2]) + endeff[1];
            objectAbove_s[2] = (orientation[2][0]*objectAbove_g[0] + orientation[2][1]*objectAbove_g[1] + orientation[2][2]* objectAbove_g[2]) + endeff[2];

            objectBelow_s[0] = (orientation[0][0]*objectBelow_g[0] + orientation[0][1]*objectBelow_g[1] + orientation[0][2]* objectBelow_g[2]) + endeff[0];
            objectBelow_s[1] = (orientation[1][0]*objectBelow_g[0] + orientation[1][1]*objectBelow_g[1] + orientation[1][2]* objectBelow_g[2]) + endeff[1];
            objectBelow_s[2] = (orientation[2][0]*objectBelow_g[0] + orientation[2][1]*objectBelow_g[1] + orientation[2][2]* objectBelow_g[2]) + endeff[2];

//             printf("[IsValidCoord] objectAbove:(%.0f %.0f %.0f) objectBelow: (%.0f %.0f %.0f)\n",objectAbove_g[0], objectAbove_g[1], objectAbove_g[2], objectBelow_g[0], objectBelow_g[1], objectBelow_g[2]);
//             printf("[IsValidCoord] objectAbove:(%u %u %u) objectBelow: (%u %u %u)\n",objectAbove_s[0], objectAbove_s[1], objectAbove_s[2], objectBelow_s[0], objectBelow_s[1], objectBelow_s[2]);

            //check if the line is a valid line segment
            pTestedCells = NULL;
            if(!IsValidLineSegment(objectBelow_s[0],objectBelow_s[1],objectBelow_s[2],objectAbove_s[0], objectAbove_s[1],objectAbove_s[2], Grid3D, pTestedCells))
            {
                if(pTestedCells == NULL)
                {
                    check_collision_time += clock() - currenttime;
                    return 0;
                }
                else
                {
                    retvalue = 0;
                }
            }
            //printf("[IsValidCoord] elbow: (%u,%u,%u)  wrist:(%u,%u,%u) endeff:(%u,%u,%u) object: (%u %u %u)\n", elbow[0],elbow[1],elbow[2],
            //        wrist[0],arm-> wrist[1], wrist[2], endeff[0], endeff[1], endeff[2], objectBelow_s[0], objectBelow_s[1], objectBelow_s[2]);
        }

    }

    check_collision_time += clock() - currenttime;
    return retvalue;
}

void EnvironmentROBARM3D::HighResGrid2LowResGrid(short unsigned int * XYZ_hr, short unsigned int * XYZ_lr)
{
    double scale = EnvROBARMCfg.GridCellWidth / EnvROBARMCfg.LowResGridCellWidth;

    XYZ_lr[0] = XYZ_hr[0] * scale;
    XYZ_lr[1] = XYZ_hr[1] * scale;
    XYZ_lr[2] = XYZ_hr[2] * scale;
}

int EnvironmentROBARM3D::cost(short unsigned int state1coord[], short unsigned int state2coord[])
{
//     if(!IsValidCoord(state1coord) || !IsValidCoord(state2coord))
//         return INFINITECOST;

#if UNIFORM_COST
    return 1*COSTMULT;
#else
//     printf("%i\n",1 * COSTMULT * (EnvROBARMCfg.Grid3D[HashEntry2->endeff[0]][HashEntry1->endeff[1]][HashEntry1->endeff[2]] + 1));
    //temporary
    return 1*COSTMULT;

#endif
}

int EnvironmentROBARM3D::cost(EnvROBARMHashEntry_t* HashEntry1, EnvROBARMHashEntry_t* HashEntry2, bool bState2IsGoal)
{
    //why does the goal return as invalid from IsValidCoord?
//     if (!bState2IsGoal)
//     {
//         if(!IsValidCoord(HashEntry1->coord,HashEntry1) || !IsValidCoord(HashEntry2->coord,HashEntry2))
//             return INFINITECOST;
//     }
//     else
//     {
//         if(!IsValidCoord(HashEntry1->coord,HashEntry1))
//             return INFINITECOST;
//     }

#if UNIFORM_COST
    return 1*COSTMULT;
#else
//     printf("%i\n",1 * COSTMULT * (EnvROBARMCfg.Grid3D[HashEntry2->endeff[0]][HashEntry1->endeff[1]][HashEntry1->endeff[2]] + 1));
    if(EnvROBARMCfg.lowres_collision_checking)
    {
        short unsigned int endeff[3];
        HighResGrid2LowResGrid(HashEntry2->endeff, endeff);
        return 1 * COSTMULT * (EnvROBARMCfg.LowResGrid3D[endeff[0]][endeff[1]][endeff[2]] + 1);
    }
    else
        return 1 * COSTMULT * (EnvROBARMCfg.Grid3D[HashEntry2->endeff[0]][HashEntry1->endeff[1]][HashEntry1->endeff[2]] + 1);
#endif
}

void EnvironmentROBARM3D::InitializeEnvConfig()
{
	//find the discretization for each angle and store the discretization
	DiscretizeAngles();
}

bool EnvironmentROBARM3D::InitializeEnvironment()
{
    short unsigned int coord[NUMOFLINKS];
    double startangles[NUMOFLINKS], angles[NUMOFLINKS];
    short unsigned int elbow[3],wrist[3],endeff[3];
    double orientation[3][3];
    int i;

    //initialize the map from Coord to StateID
    EnvROBARM.HashTableSize = 32*1024; //should be power of two
    EnvROBARM.Coord2StateIDHashTable = new vector<EnvROBARMHashEntry_t*>[EnvROBARM.HashTableSize];

    //initialize the map from StateID to Coord
    EnvROBARM.StateID2CoordTable.clear();

    //initialize the angles of the start states
    for(i = 0; i < NUMOFLINKS; i++)
        startangles[i] = PI_CONST*(EnvROBARMCfg.LinkStartAngles_d[i]/180.0);

    ComputeCoord(startangles, coord);
    ComputeContAngles(coord, angles);
    ComputeEndEffectorPos(angles, endeff, wrist, elbow, orientation);

    //create the start state
    EnvROBARM.startHashEntry = CreateNewHashEntry(coord, NUMOFLINKS, endeff, 0, orientation);

    //check if start position is valid
    if(!IsValidCoord(coord, endeff, wrist, elbow, orientation))
    {
//         printf("Start Pos: %1.2f %1.2f %1.2f %1.2f %1.2f %1.2f %1.2f\n",startangles[0],startangles[1],startangles[2],startangles[3],startangles[4],startangles[5],startangles[6]);
        printf("[InitializeEnvironment] Start Hash Entry is invalid\n");
        return false;
    }

    //create the goal state
    //initialize the coord of goal state
    for(i = 0; i < NUMOFLINKS; i++)
        coord[i] = 0;

    EnvROBARM.goalHashEntry = CreateNewHashEntry(coord, NUMOFLINKS, endeff, 0, NULL);

    //create the goal state
    if(EnvROBARMCfg.bGoalIsSet)
    {
        EnvROBARM.goalHashEntry->endeff[0] = EnvROBARMCfg.EndEffGoals_c[0][0];
        EnvROBARM.goalHashEntry->endeff[1] = EnvROBARMCfg.EndEffGoals_c[0][1];
        EnvROBARM.goalHashEntry->endeff[2] = EnvROBARMCfg.EndEffGoals_c[0][2];
    }

    //for now heuristics are not set
    EnvROBARM.Heur = NULL;

    return true;
}
//----------------------------------------------------------------------


/*------------------------------------------------------------------------*/
                /* Interface with Outside Functions */
/*------------------------------------------------------------------------*/
EnvironmentROBARM3D::EnvironmentROBARM3D()
{
    //initializations
    EnvROBARMCfg.bInitialized = false;
    EnvROBARMCfg.bGoalIsSet = false;
    EnvROBARMCfg.use_DH = 1;
    EnvROBARMCfg.enforce_motor_limits = 1;
    EnvROBARMCfg.dijkstra_heuristic = 1;
    EnvROBARMCfg.endeff_check_only = 0;
    EnvROBARMCfg.padding = 0.06;
    EnvROBARMCfg.smoothing_weight = 0.0;
    EnvROBARMCfg.use_smooth_actions = 1;
    EnvROBARMCfg.gripper_orientation_moe = .0175; // 0.0125;
    EnvROBARMCfg.object_grasped = 0;
    EnvROBARMCfg.grasped_object_length_m = .1;
    EnvROBARMCfg.enforce_upright_gripper = 0;
    EnvROBARMCfg.goal_moe_m = .1;
    EnvROBARMCfg.JointSpaceGoal = 0;
    EnvROBARMCfg.checkEndEffGoalOrientation = 0;
    EnvROBARMCfg.HighResActionsThreshold_c = 20;
    EnvROBARMCfg.lowres_collision_checking = 0;
    EnvROBARMCfg.multires_succ_actions = 1;

    EnvROBARMCfg.ObstacleCost = 10;
    EnvROBARMCfg.medObstacleCost = 5;
    EnvROBARMCfg.lowObstacleCost = 2;

    EnvROBARMCfg.medCostRadius_c = 3;
    EnvROBARMCfg.lowCostRadius_c = 3;

    EnvROBARMCfg.variable_cell_costs = 1;

    EnvROBARMCfg.CellsPerAction = -1;

    //default goal orientation margin of error for each element in rotation matrix
    for(int i = 0; i < 3; i++)
        for(int j = 0; j < 3; j++)
            EnvROBARMCfg.GoalOrientationMOE[i][j] = .02;

    EnvROBARMCfg.GoalRPY_MOE[0] = .2;
    EnvROBARMCfg.GoalRPY_MOE[1] = .2;
    EnvROBARMCfg.GoalRPY_MOE[2] = .2;
    //parse the parameter file - temporary
//     char parFile[] = "params.cfg";
//     FILE* fCfg = fopen(parFile, "r");
//     if(fCfg == NULL)
// 	printf("ERROR: unable to open %s.....using defaults.\n", parFile);
// 
//     ReadParamsFile(fCfg);
//     fclose(fCfg);
}

bool EnvironmentROBARM3D::InitializeEnv(const char* sEnvFile)
{
    //parse the parameter file - temporary
    char parFile[] = "params.cfg";
    FILE* fCfg = fopen(parFile, "r");
    if(fCfg == NULL)
        printf("ERROR: unable to open %s.....using defaults.\n", parFile);

    ReadParamsFile(fCfg);
    fclose(fCfg);

    //parse the configuration file
    fCfg = fopen(sEnvFile, "r");
    if(fCfg == NULL)
    {
        printf("ERROR: unable to open %s\n", sEnvFile);
        exit(1);
    }
    ReadConfiguration(fCfg);

    //initialize forward kinematics
    if (!EnvROBARMCfg.use_DH)
        //start ros node
        InitializeKinNode();
    else
        //pre-compute DH Transformations
        ComputeDHTransformations();

    //if goal is in joint space, do the FK on the goal joint angles
    if(EnvROBARMCfg.JointSpaceGoal)
    {
        short unsigned int wrist[3],elbow[3],endeff[3];
        double goalangles[NUMOFLINKS];
        double orientation[3][3];

        // convert goal angles to radians
        for(int i = 0; i < NUMOFLINKS; i++)
            goalangles[i] = PI_CONST*(EnvROBARMCfg.LinkGoalAngles_d[i]/180.0);

        ComputeEndEffectorPos(goalangles, endeff, wrist, elbow, orientation);
        EnvROBARMCfg.EndEffGoalX_c = endeff[0];
        EnvROBARMCfg.EndEffGoalY_c = endeff[1];
        EnvROBARMCfg.EndEffGoalZ_c = endeff[2];
    }

    //initialize other parameters of the environment
    InitializeEnvConfig();

    //initialize Environment
    if(InitializeEnvironment() == false)
        return false;

    //pre-compute action-to-action costs
    ComputeActionCosts();

    //compute the cost per cell to be used by heuristic
    ComputeCostPerCell();

    //set goals
    SetEndEffGoals(EnvROBARMCfg.EndEffGoals_m, 0, EnvROBARMCfg.nEndEffGoals,1);

#if VERBOSE
    //output environment data
    PrintConfiguration();

    printf("Use DH Convention for FK: %i\n", EnvROBARMCfg.use_DH);
    printf("Enforce Motor Limits: %i\n", EnvROBARMCfg.enforce_motor_limits);
    printf("Use Dijkstra for Heuristic Function: %i\n", EnvROBARMCfg.dijkstra_heuristic);
    printf("EndEffector Collision Check Only: %i\n", EnvROBARMCfg.endeff_check_only);
    printf("Smoothness Weight: %.3f\n", EnvROBARMCfg.smoothing_weight);
    printf("Obstacle Padding(meters): %.2f\n", EnvROBARMCfg.padding);
    printf("\n");

    //output successor actions
    OutputActions();

    //output action costs
    if(EnvROBARMCfg.use_smooth_actions)
        OutputActionCostTable();
#endif

    //set Environment is Initialized flag(so fk can be used)
    EnvROBARMCfg.bInitialized = 1;

    return true;
}

bool EnvironmentROBARM3D::InitEnvFromFilePtr(FILE* eCfg, FILE* pCfg)
{
    //parse the parameter file
    ReadParamsFile(pCfg);

    //parse the environment configuration file
    ReadConfiguration(eCfg);

    //initialize forward kinematics
    if (!EnvROBARMCfg.use_DH)
        //start ros node
        InitializeKinNode();
    else
        //pre-compute DH Transformations
        ComputeDHTransformations();

    //if goal is in joint space, do the FK on the goal joint angles
    if(EnvROBARMCfg.JointSpaceGoal)
    {
        short unsigned int wrist[3],elbow[3],endeff[3];
        double goalangles[NUMOFLINKS];
        double orientation[3][3];

        // convert goal angles to radians
        for(int i = 0; i < NUMOFLINKS; i++)
            goalangles[i] = PI_CONST*(EnvROBARMCfg.LinkGoalAngles_d[i]/180.0);

        ComputeEndEffectorPos(goalangles, endeff, wrist, elbow, orientation);
        EnvROBARMCfg.EndEffGoalX_c = endeff[0];
        EnvROBARMCfg.EndEffGoalY_c = endeff[1];
        EnvROBARMCfg.EndEffGoalZ_c = endeff[2];
    }

    //initialize other parameters of the environment
    InitializeEnvConfig();

    //initialize Environment
    if(InitializeEnvironment() == false)
        return false;

    //pre-compute action-to-action costs
    ComputeActionCosts();

    //compute the cost per cell to be used by heuristic
    ComputeCostPerCell();

#if VERBOSE
    //output environment data
    PrintConfiguration();

    printf("Use DH Convention for FK: %i\n", EnvROBARMCfg.use_DH);
    printf("Enforce Motor Limits: %i\n", EnvROBARMCfg.enforce_motor_limits);
    printf("Use Dijkstra for Heuristic Function: %i\n", EnvROBARMCfg.dijkstra_heuristic);
    printf("EndEffector Collision Check Only: %i\n", EnvROBARMCfg.endeff_check_only);
    printf("Smoothness Weight: %.3f\n", EnvROBARMCfg.smoothing_weight);
    printf("Obstacle Padding(meters): %.2f\n", EnvROBARMCfg.padding);
    printf("\n");

    //output successor actions
    OutputActions();

    //output action costs
    if(EnvROBARMCfg.use_smooth_actions)
        OutputActionCostTable();
#endif

    //set Environment is Initialized flag(so fk can be used)
    EnvROBARMCfg.bInitialized = 1;

    printf("Environment has been initialized.\n");
    return true;
}

bool EnvironmentROBARM3D::InitializeMDPCfg(MDPConfig *MDPCfg)
{
	//initialize MDPCfg with the start and goal ids	
	MDPCfg->goalstateid = EnvROBARM.goalHashEntry->stateID;
	MDPCfg->startstateid = EnvROBARM.startHashEntry->stateID;

	return true;
}

int EnvironmentROBARM3D::GetFromToHeuristic(int FromStateID, int ToStateID)
{
    int h;

#if USE_HEUR==0
    return 0;
#endif

#if DEBUG
    if(FromStateID >= (int)EnvROBARM.StateID2CoordTable.size() || ToStateID >= (int)EnvROBARM.StateID2CoordTable.size())
    {
        printf("ERROR in EnvROBARM... function: stateID illegal\n");
	exit(1);
    }
#endif

    //get X, Y, Z for the state
    EnvROBARMHashEntry_t* FromHashEntry = EnvROBARM.StateID2CoordTable[FromStateID];
    EnvROBARMHashEntry_t* ToHashEntry = EnvROBARM.StateID2CoordTable[ToStateID];

    //Dijkstra's algorithm
    if(EnvROBARMCfg.dijkstra_heuristic)
    {
        //lowres collision checking
        if(EnvROBARMCfg.lowres_collision_checking)
        {
            short unsigned int endeff[3];
            HighResGrid2LowResGrid(FromHashEntry->endeff, endeff);
            h = EnvROBARM.Heur[XYZTO3DIND(endeff[0], endeff[1], endeff[2])];

            double dist_to_goal_c = sqrt((FromHashEntry->endeff[0]-ToHashEntry->endeff[0])*(FromHashEntry->endeff[0]-ToHashEntry->endeff[0]) + 
                                (FromHashEntry->endeff[1]-ToHashEntry->endeff[1])*(FromHashEntry->endeff[1]-ToHashEntry->endeff[1]) + 
                                (FromHashEntry->endeff[2]-ToHashEntry->endeff[2])*(FromHashEntry->endeff[2]-ToHashEntry->endeff[2]));

            if(dist_to_goal_c*EnvROBARMCfg.GridCellWidth < MAX_EUCL_DIJK_m)
            {
                double cost_per_mm = EnvROBARMCfg.cost_per_cell / (EnvROBARMCfg.GridCellWidth * 1000);
                int h_eucl_cost = dist_to_goal_c * EnvROBARMCfg.GridCellWidth * 1000 * cost_per_mm;

                // printf("h: %i, eucl_cost: %i, cost_per_mm: %3.2f\n",h, h_eucl_cost,cost_per_mm);

                if (h_eucl_cost > h)
                    h =  h_eucl_cost;
            }
        }
        //highres collision checking
        else
            h = EnvROBARM.Heur[XYZTO3DIND(FromHashEntry->endeff[0], FromHashEntry->endeff[1], FromHashEntry->endeff[2])];
    }

    //euclidean distance
    else 
        h = (EnvROBARMCfg.cost_per_cell)*sqrt((FromHashEntry->endeff[0]-ToHashEntry->endeff[0])*(FromHashEntry->endeff[0]-ToHashEntry->endeff[0]) + 
                (FromHashEntry->endeff[1]-ToHashEntry->endeff[1])*(FromHashEntry->endeff[1]-ToHashEntry->endeff[1]) + 
                (FromHashEntry->endeff[2]-ToHashEntry->endeff[2])*(FromHashEntry->endeff[2]-ToHashEntry->endeff[2]));

//     printf("GetFromToHeuristic(#%i(%i,%i,%i) -> #%i(%i,%i,%i)) H cost: %i\n",FromStateID, FromHashEntry->endeff[0], FromHashEntry->endeff[1], FromHashEntry->endeff[2], ToStateID, ToHashEntry->endeff[0], ToHashEntry->endeff[1], ToHashEntry->endeff[2], h);
    return h;
}

int EnvironmentROBARM3D::GetGoalHeuristic(int stateID)
{
#if USE_HEUR==0
	return 0;
#endif

#if DEBUG
	if(stateID >= (int)EnvROBARM.StateID2CoordTable.size())
	{
		printf("ERROR in EnvROBARM... function: stateID illegal\n");
		exit(1);
	}
#endif


	//define this function if it used in the planner (heuristic forward search would use it)

	printf("ERROR in EnvROBARM..function: GetGoalHeuristic is undefined\n");
	exit(1);
}

int EnvironmentROBARM3D::GetStartHeuristic(int stateID)
{
#if USE_HEUR==0
	return 0;
#endif

#if DEBUG
	if(stateID >= (int)EnvROBARM.StateID2CoordTable.size())
	{
		printf("ERROR in EnvROBARM... function: stateID illegal\n");
		exit(1);
	}
#endif

	//define this function if it used in the planner (heuristic backward search would use it)
	printf("ERROR in EnvROBARM... function: GetStartHeuristic is undefined\n");
	exit(1);

	return 0;
}

double EnvironmentROBARM3D::GetEpsilon()
{
    return EnvROBARMCfg.epsilon;
}

int EnvironmentROBARM3D::SizeofCreatedEnv()
{
	return EnvROBARM.StateID2CoordTable.size();
	
}

void EnvironmentROBARM3D::PrintState(int stateID, bool bVerbose, FILE* fOut /*=NULL*/)
{
    bool bLocal = false;

#if DEBUG
    if(stateID >= (int)EnvROBARM.StateID2CoordTable.size())
    {
	printf("ERROR in EnvROBARM... function: stateID illegal (2)\n");
	exit(1);
    }
#endif

    if(fOut == NULL)
        fOut = stdout;

    EnvROBARMHashEntry_t* HashEntry = EnvROBARM.StateID2CoordTable[stateID];

    bool bGoal = false;
    if(stateID == EnvROBARM.goalHashEntry->stateID)
        bGoal = true;

    if(stateID == EnvROBARM.goalHashEntry->stateID && bVerbose)
    {
	fprintf(fOut, "the state is a goal state\n");
	bGoal = true;
    }

    if(bLocal)
    {
	printangles(fOut, HashEntry->coord, bGoal, bVerbose, true);

    }
    else
    {
	if(!bGoal) //remove this if statement
	{
// 	    printangles(fOut, HashEntry->coord, bGoal, bVerbose, false);
	    PrintAnglesWithAction(fOut, HashEntry, bGoal, bVerbose, false);
// 	    fprintf(fOut, "  action: %i\n", HashEntry->action);
	}
	else
            PrintAnglesWithAction(fOut, HashEntry, bGoal, bVerbose, false);
//             printangles(fOut, EnvROBARMCfg.goalcoords, bGoal, bVerbose, false);
    }
}

//get the goal as a successor of source state at given cost
//if costtogoal = -1 then the succ is chosen
void EnvironmentROBARM3D::PrintSuccGoal(int SourceStateID, int costtogoal, bool bVerbose, bool bLocal /*=false*/, FILE* fOut /*=NULL*/)
{
    printf("[PrintSuccGoal] WARNING: This function has not been updated to deal with multiple goals.\n");

    short unsigned int succcoord[NUMOFLINKS];
    double angles[NUMOFLINKS],orientation[3][3];
    short unsigned int endeff[3],wrist[3],elbow[3];
    int i, inc;

    if(fOut == NULL)
        fOut = stdout;

    EnvROBARMHashEntry_t* HashEntry = EnvROBARM.StateID2CoordTable[SourceStateID];

    //default coords of successor
    for(i = 0; i < NUMOFLINKS; i++)
        succcoord[i] = HashEntry->coord[i];	

    //iterate through successors of s
    for (i = 0; i < NUMOFLINKS; i++)
    {
        //increase and decrease in ith angle
        for(inc = -1; inc < 2; inc = inc+2)
        {
            if(inc == -1)
            {
                if(HashEntry->coord[i] == 0)
                    succcoord[i] = EnvROBARMCfg.anglevals[i]-1;
                else
                    succcoord[i] = HashEntry->coord[i] + inc;
            }
            else
            {
                succcoord[i] = (HashEntry->coord[i] + inc)%
                EnvROBARMCfg.anglevals[i];
            }

            ComputeContAngles(succcoord, angles);
            ComputeEndEffectorPos(angles,endeff,wrist,elbow,orientation);

            //skip invalid successors
            if(!IsValidCoord(succcoord,endeff,wrist,elbow,orientation))
                continue;

            if(endeff[0] == EnvROBARMCfg.EndEffGoalX_c && endeff[1] == EnvROBARMCfg.EndEffGoalY_c && endeff[2] ==  EnvROBARMCfg.EndEffGoalZ_c)
            {
                if(cost(HashEntry->coord,succcoord) == costtogoal || costtogoal == -1)
                {

                    if(bVerbose)
                        fprintf(fOut, "the state is a goal state\n");
                    printangles(fOut, succcoord, true, bVerbose, bLocal);
                    return;
                }
            }
        }

        //restore it back
        succcoord[i] = HashEntry->coord[i];
    }
}

void EnvironmentROBARM3D::PrintEnv_Config(FILE* fOut)
{
	//implement this if the planner needs to print out EnvROBARM. configuration
	
	printf("ERROR in EnvROBARM... function: PrintEnv_Config is undefined\n");
	exit(1);
}

void EnvironmentROBARM3D::PrintHeader(FILE* fOut)
{
    fprintf(fOut, "%d\n", NUMOFLINKS);
    for(int i = 0; i < NUMOFLINKS; i++)
        fprintf(fOut, "%.3f ", EnvROBARMCfg.LinkLength_m[i]);
    fprintf(fOut, "\n");
}

void EnvironmentROBARM3D::GetSuccs(int SourceStateID, vector<int>* SuccIDV, vector<int>* CostV)
{
    short unsigned int succcoord[NUMOFLINKS];
    short unsigned int goal_moe_c = EnvROBARMCfg.goal_moe_m / EnvROBARMCfg.GridCellWidth + .999999;
    short unsigned int k, indx, wrist[3], elbow[3], endeff[3];
    double orientation [3][3];
    int i, inc, a, correct_orientation;
    double angles[NUMOFLINKS], s_angles[NUMOFLINKS];
    double roll, pitch, yaw;

    //to support two sets of succesor actions
    int actions_i_min = 0, actions_i_max = EnvROBARMCfg.nLowResActions;

    //clear the successor array
    SuccIDV->clear();
    CostV->clear();

    //goal state should be absorbing
    if(SourceStateID == EnvROBARM.goalHashEntry->stateID)
        return;

    //get X, Y, Z for the state
    EnvROBARMHashEntry_t* HashEntry = EnvROBARM.StateID2CoordTable[SourceStateID];
    ComputeContAngles(HashEntry->coord, s_angles);

    //default coords of successor
    for(i = 0; i < NUMOFLINKS; i++)
        succcoord[i] = HashEntry->coord[i];

    ComputeContAngles(succcoord, angles);

    //check if cell is close to enough to goal to use higher resolution actions
    if(EnvROBARMCfg.multires_succ_actions)
    {
        if (GetEuclideanDistToGoal((HashEntry->endeff)) <= EnvROBARMCfg.HighResActionsThreshold_c)
        {
            actions_i_max = EnvROBARMCfg.nSuccActions;
            // printf("[GetSuccs] Using HighRes Actions. Distance to Goal: %i\n",GetEuclideanDistToGoal(HashEntry->endeff));
        }
    }

    //iterate through successors of s (possible actions)
    for (i = actions_i_min; i < actions_i_max; i++)
    {
        //increase and decrease in ith angle
        for(inc = -1; inc < 2; inc = inc+2)
        {
            if(inc == -1)
            {
                for(a = 0; a < NUMOFLINKS; a++)
                {
                    //if the joint is at 0deg and the next action will decrement it
                    if(HashEntry->coord[a] == 0 && EnvROBARMCfg.SuccActions[i][a] != 0)
                        succcoord[a] =  EnvROBARMCfg.anglevals[a] - EnvROBARMCfg.SuccActions[i][a];
                    //the joint's current position, when decremented by n degrees will go below 0
                    else if(HashEntry->coord[a] - EnvROBARMCfg.SuccActions[i][a] < 0)
                        succcoord[a] =  EnvROBARMCfg.anglevals[a] + (HashEntry->coord[a] - EnvROBARMCfg.SuccActions[i][a]);
                    else
                        succcoord[a] = HashEntry->coord[a] - EnvROBARMCfg.SuccActions[i][a];
                }
            }
            else
            {
                for(a = 0; a < NUMOFLINKS; a++)
                    succcoord[a] = (HashEntry->coord[a] + int(EnvROBARMCfg.SuccActions[i][a])) % EnvROBARMCfg.anglevals[a];
            }

            //get the successor
            EnvROBARMHashEntry_t* OutHashEntry;
            bool bSuccisGoal = false;

            //have to create a new entry
            ComputeContAngles(succcoord, angles);

            //get forward kinematics
            if(ComputeEndEffectorPos(angles, endeff, wrist, elbow, orientation) == false)
            {
                continue;
            }

            //do collision checking
            if(!IsValidCoord(succcoord, endeff, wrist, elbow, orientation))
            {
                continue;
            }

            //check if within goal_moe_c cells of the goal
            for(k = 0; k < EnvROBARMCfg.nEndEffGoals; k++)
            {
                if(fabs(endeff[0] - EnvROBARMCfg.EndEffGoals_c[k][0]) < goal_moe_c && 
                   fabs(endeff[1] - EnvROBARMCfg.EndEffGoals_c[k][1]) < goal_moe_c && 
                   fabs(endeff[2] - EnvROBARMCfg.EndEffGoals_c[k][2]) < goal_moe_c)
                {
                    //check if end effector has the correct orientation in the shoulder frame
                    if(EnvROBARMCfg.checkEndEffGoalOrientation)
                    {
                        correct_orientation = 1;
//                         indx = 0;
//                         for (int x = 0; x < 3; x++)
//                         {
//                             for (int y = 0; y < 3; y++)
//                             {
//                                 if(fabs(orientation[x][y] - EnvROBARMCfg.EndEffGoalOrientations[k][indx]) > EnvROBARMCfg.GoalOrientationMOE[x][y])
//                                 {
//                                     correct_orientation = 0;
//                                     break;
//                                 }
//                                 indx++;
//                             }
//                             if(correct_orientation == 0)
//                                 break;
//                         }

                        // get roll pitch and yaw
                        getRPY(orientation, &roll, &pitch, &yaw);

                        //compare RPY to goal RPY and see if it is within the MOE 
                        if(fabs(roll - EnvROBARMCfg.EndEffGoalRPY[k][0]) > EnvROBARMCfg.GoalRPY_MOE[0] ||
                           fabs(pitch - EnvROBARMCfg.EndEffGoalRPY[k][1]) > EnvROBARMCfg.GoalRPY_MOE[1] ||
                           fabs(yaw - EnvROBARMCfg.EndEffGoalRPY[k][2]) > EnvROBARMCfg.GoalRPY_MOE[2])
                                correct_orientation = 0;

                        if(correct_orientation == 1)
                        {
                            bSuccisGoal = true;
                            // printf("goal succ is generated\n");
                            for (int j = 0; j < NUMOFLINKS; j++)
                            {
                                EnvROBARMCfg.goalcoords[j] = succcoord[j];
                                EnvROBARM.goalHashEntry->coord[j] = succcoord[j];
                            }
                            EnvROBARM.goalHashEntry->endeff[0] = EnvROBARMCfg.EndEffGoals_c[k][0];
                            EnvROBARM.goalHashEntry->endeff[1] = EnvROBARMCfg.EndEffGoals_c[k][1];
                            EnvROBARM.goalHashEntry->endeff[2] = EnvROBARMCfg.EndEffGoals_c[k][2];
                            EnvROBARM.goalHashEntry->action = i;
                        }
                    }
                    //3DoF goal position
                    else
                    {
                        bSuccisGoal = true;
                        // printf("goal succ is generated\n");
                        for (int j = 0; j < NUMOFLINKS; j++)
                        {
                            EnvROBARMCfg.goalcoords[j] = succcoord[j];
                            EnvROBARM.goalHashEntry->coord[j] = succcoord[j];
                        }
                        EnvROBARM.goalHashEntry->endeff[0] = EnvROBARMCfg.EndEffGoals_c[k][0];
                        EnvROBARM.goalHashEntry->endeff[1] = EnvROBARMCfg.EndEffGoals_c[k][1];
                        EnvROBARM.goalHashEntry->endeff[2] = EnvROBARMCfg.EndEffGoals_c[k][2];
                        EnvROBARM.goalHashEntry->action = i;
                    }
                }
            }
            //check if hash entry already exists, if not then create one
            if((OutHashEntry = GetHashEntry(succcoord, NUMOFLINKS, i, bSuccisGoal)) == NULL)
            {
                OutHashEntry = CreateNewHashEntry(succcoord, NUMOFLINKS, endeff, i, orientation);
            }

            SuccIDV->push_back(OutHashEntry->stateID);
            CostV->push_back(cost(HashEntry,OutHashEntry,bSuccisGoal) + GetFromToHeuristic(OutHashEntry->stateID, EnvROBARM.goalHashEntry->stateID) + EnvROBARMCfg.ActiontoActionCosts[HashEntry->action][OutHashEntry->action]);           
            // printf("%i %i %i --> %i %i %i,  g: %i  h: %i\n",HashEntry->endeff[0],HashEntry->endeff[1],HashEntry->endeff[2],endeff[0],endeff[1],endeff[2],cost(HashEntry,OutHashEntry,bSuccisGoal),GetFromToHeuristic(OutHashEntry->stateID, EnvROBARM.goalHashEntry->stateID)); 
        }
    }
}

void EnvironmentROBARM3D::GetPreds(int TargetStateID, vector<int>* PredIDV, vector<int>* CostV)
{

    printf("ERROR in EnvROBARM... function: GetPreds is undefined\n");
    exit(1);
}

void EnvironmentROBARM3D::SetAllActionsandAllOutcomes(CMDPSTATE* state)
{


    printf("ERROR in EnvROBARM..function: SetAllActionsandOutcomes is undefined\n");
    exit(1);
}

void EnvironmentROBARM3D::SetAllPreds(CMDPSTATE* state)
{
	//implement this if the planner needs access to predecessors
	
    printf("ERROR in EnvROBARM... function: SetAllPreds is undefined\n");
    exit(1);
}

int EnvironmentROBARM3D::GetEdgeCost(int FromStateID, int ToStateID)
{

#if DEBUG
	if(FromStateID >= (int)EnvROBARM.StateID2CoordTable.size() 
		|| ToStateID >= (int)EnvROBARM.StateID2CoordTable.size())
	{
		printf("ERROR in EnvROBARM... function: stateID illegal\n");
		exit(1);
	}
#endif

	//get X, Y for the state
	EnvROBARMHashEntry_t* FromHashEntry = EnvROBARM.StateID2CoordTable[FromStateID];
	EnvROBARMHashEntry_t* ToHashEntry = EnvROBARM.StateID2CoordTable[ToStateID];
	

	return cost(FromHashEntry->coord, ToHashEntry->coord);

}

bool EnvironmentROBARM3D::AreEquivalent(int State1ID, int State2ID)
{
    EnvROBARMHashEntry_t* HashEntry1 = EnvROBARM.StateID2CoordTable[State1ID];
    EnvROBARMHashEntry_t* HashEntry2 = EnvROBARM.StateID2CoordTable[State2ID];

    for(int i = 0; i < NUMOFLINKS; i++)
    {
        if(HashEntry1->coord[i] != HashEntry2->coord[i])
            return false;
    }
    if(HashEntry1->action != HashEntry2->action)
        return false;

    return true;
}

bool EnvironmentROBARM3D::SetEndEffGoals(double** EndEffGoals, int goal_type, int num_goals, bool bComputeHeuristic)
{
    if(EnvROBARMCfg.bGoalIsSet)
    {
        //delete the old goal array
        for (int i = 0; i < EnvROBARMCfg.nEndEffGoals; i++)
        {
            delete [] EnvROBARMCfg.EndEffGoals_c[i];
            delete [] EnvROBARMCfg.EndEffGoalOrientations[i];
        }
        delete [] EnvROBARMCfg.EndEffGoals_c;
        delete [] EnvROBARMCfg.EndEffGoalOrientations;

        EnvROBARMCfg.EndEffGoals_c = NULL;
        EnvROBARMCfg.EndEffGoalOrientations = NULL;

        EnvROBARM.goalHashEntry = NULL;
        EnvROBARMCfg.bGoalIsSet = false;
    }

    //allocate memory
    EnvROBARMCfg.nEndEffGoals = num_goals;
    EnvROBARMCfg.EndEffGoals_c = new short unsigned int * [num_goals];
    EnvROBARMCfg.EndEffGoalOrientations = new double * [num_goals];
    EnvROBARMCfg.EndEffGoalRPY = new double * [num_goals];
    for (int i = 0; i < num_goals; i++)
    {
        EnvROBARMCfg.EndEffGoals_c[i] = new short unsigned int [3];
        EnvROBARMCfg.EndEffGoalOrientations[i] = new double [SIZE_ROTATION_MATRIX];
        EnvROBARMCfg.EndEffGoalRPY[i] = new double [3];
    }

    //Goal is in cartesian coordinates in world frame (meters)
    if(goal_type == 0)
    {
        //loop through all the goal positions
        for(int i = 0; i < num_goals; i++)
        {
            //convert goal position from meters to cells
            ContXYZ2Cell(EndEffGoals[i][0],EndEffGoals[i][1],EndEffGoals[i][2], &(EnvROBARMCfg.EndEffGoals_c[i][0]), &(EnvROBARMCfg.EndEffGoals_c[i][1]), &(EnvROBARMCfg.EndEffGoals_c[i][2]));

            //input orientation (rotation matrix) of each goal position 
            for(int k = 0; k < 9; k++)
            {
                EnvROBARMCfg.EndEffGoalOrientations[i][k] = EndEffGoals[i][k+3];
            }

            //RPY
            for (int k = 0; k < 3; k++)
                EnvROBARMCfg.EndEffGoalRPY[i][k] = EndEffGoals[i][k+3];
        }
        //set goalangle to invalid number
        EnvROBARMCfg.LinkGoalAngles_d[0] = INVALID_NUMBER;
    }
    //Goal is a joint space vector in radians
    else if(goal_type == 1)
    {
        for(int i = 0; i < 7; i++)
            EnvROBARMCfg.LinkGoalAngles_d[i] = DEG2RAD(EndEffGoals[0][i]);

        //so the goal's location (forward kinematics) can be calculated after the initialization completes
        EnvROBARMCfg.JointSpaceGoal = 1;
    }

    else
    {
        printf("[SetEndEffGoal] Invalid type of goal vector.\n");
    }

    //check if goal positions are valid
    for(int i = 0; i < EnvROBARMCfg.nEndEffGoals; i++)
    {
        if(EnvROBARMCfg.EndEffGoals_c[i][0] >= EnvROBARMCfg.EnvWidth_c ||
           EnvROBARMCfg.EndEffGoals_c[i][1] >= EnvROBARMCfg.EnvHeight_c ||
           EnvROBARMCfg.EndEffGoals_c[i][2] >= EnvROBARMCfg.EnvDepth_c)
        {
            printf("End effector goal position(%u %u %u) is out of bounds.\n",EnvROBARMCfg.EndEffGoals_c[i][0],EnvROBARMCfg.EndEffGoals_c[i][1],EnvROBARMCfg.EndEffGoals_c[i][2]);
            return false;
        }

        if(EnvROBARMCfg.Grid3D[EnvROBARMCfg.EndEffGoals_c[i][0]][EnvROBARMCfg.EndEffGoals_c[i][1]][EnvROBARMCfg.EndEffGoals_c[i][2]] >= EnvROBARMCfg.ObstacleCost)
        {
            printf("End effector goal position(%u %u %u) is invalid\n",EnvROBARMCfg.EndEffGoals_c[i][0],EnvROBARMCfg.EndEffGoals_c[i][1],EnvROBARMCfg.EndEffGoals_c[i][2]);
            exit(1);
        }
    }

    if(EnvROBARMCfg.cost_per_cell == -1)
        ComputeCostPerCell();

    //pre-compute heuristics with new goal
    if(EnvROBARMCfg.dijkstra_heuristic && bComputeHeuristic)
        ComputeHeuristicValues();


    EnvROBARM.goalHashEntry->endeff[0] = EnvROBARMCfg.EndEffGoals_c[0][0];
    EnvROBARM.goalHashEntry->endeff[1] = EnvROBARMCfg.EndEffGoals_c[0][1];
    EnvROBARM.goalHashEntry->endeff[2] = EnvROBARMCfg.EndEffGoals_c[0][2];

    EnvROBARMCfg.bGoalIsSet = true;
    return true;
}

bool EnvironmentROBARM3D::SetStartJointConfig(double angles[NUMOFLINKS], bool bRad)
{
    double startangles[NUMOFLINKS];
    short unsigned int elbow[3],wrist[3];

    //set initial joint configuration
    if(bRad) //input is in radians
    {
        for(unsigned int i = 0; i < NUMOFLINKS; i++)
        {
            if(angles[i] < 0)
                EnvROBARMCfg.LinkStartAngles_d[i] = RAD2DEG(angles[i] + PI_CONST*2); //fmod(RAD2DEG(angles[i]),360.0);
            else
                EnvROBARMCfg.LinkStartAngles_d[i] = RAD2DEG(angles[i]);
        }
    }
    else //input is in degrees
    {
        for(unsigned int i = 0; i < NUMOFLINKS; i++)
        {
            if(angles[i] < 0)
                EnvROBARMCfg.LinkStartAngles_d[i] =  angles[i] + 360.0; //fmod(angles[i],360.0);
            else
                EnvROBARMCfg.LinkStartAngles_d[i] =  angles[i];
        }
    }

    //initialize the map from StateID to Coord
//     EnvROBARM.StateID2CoordTable.clear();

    //initialize the angles of the start states
    for(int i = 0; i < NUMOFLINKS; i++)
        startangles[i] = DEG2RAD(EnvROBARMCfg.LinkStartAngles_d[i]);

    //compute arm position in environment
    ComputeCoord(startangles, EnvROBARM.startHashEntry->coord);
    ComputeEndEffectorPos(startangles, EnvROBARM.startHashEntry->endeff,wrist,elbow,EnvROBARM.startHashEntry->orientation);

    //check if starting position is valid
    if(!IsValidCoord(EnvROBARM.startHashEntry->coord,EnvROBARM.startHashEntry->endeff,wrist,elbow,EnvROBARM.startHashEntry->orientation))
    {
        printf("Start Pos: %1.2f %1.2f %1.2f %1.2f %1.2f %1.2f %1.2f\n",startangles[0],startangles[1],startangles[2],startangles[3],startangles[4],startangles[5],startangles[6]);
        printf("Start: EndEff (%i %i %i)\n",EnvROBARM.startHashEntry->endeff[0],EnvROBARM.startHashEntry->endeff[1],EnvROBARM.startHashEntry->endeff[2]);
        printf("Start Hash Entry is invalid\n");
        return false;
    }

    return true;
}

void EnvironmentROBARM3D::StateID2Angles(int stateID, double* angles_r)
{
    bool bGoal = false;
    EnvROBARMHashEntry_t* HashEntry = EnvROBARM.StateID2CoordTable[stateID];

    if(stateID == EnvROBARM.goalHashEntry->stateID)
        bGoal = true;

    if(bGoal)
        ComputeContAngles(EnvROBARMCfg.goalcoords, angles_r);
    else
        ComputeContAngles(HashEntry->coord, angles_r);

    //convert angles from positive values in radians (from 0->6.28) to centered around 0 (not really needed)
    for (int i = 0; i < NUMOFLINKS; i++)
    {
        if(angles_r[i] >= PI_CONST)
            angles_r[i] = -2.0*PI_CONST + angles_r[i];
    }
}

void EnvironmentROBARM3D::AddObstaclesToEnv(double**obstacles, int numobstacles)
{
    double obs[6], startangles[NUMOFLINKS], orientation[3][3];
    short unsigned int endeff[3],elbow[3],wrist[3];

    for(int i = 0; i < numobstacles; i++)
    {
        for(int p = 0; p < 6; p++)
            obs[p] = obstacles[i][p];

        AddObstacleToGrid(obs,0, EnvROBARMCfg.Grid3D, EnvROBARMCfg.GridCellWidth);

        if(EnvROBARMCfg.lowres_collision_checking)
            AddObstacleToGrid(obs,0, EnvROBARMCfg.LowResGrid3D, EnvROBARMCfg.LowResGridCellWidth);
    }

    //pre-compute heuristics
    if(EnvROBARMCfg.dijkstra_heuristic)
        ComputeHeuristicValues();

    ComputeCoord(startangles, EnvROBARM.startHashEntry->coord);
    ComputeEndEffectorPos(startangles, endeff, wrist, elbow, orientation);

    //check if the starting position and goal are still valid
    if(!IsValidCoord(EnvROBARM.startHashEntry->coord,EnvROBARM.startHashEntry->endeff,wrist,elbow,EnvROBARM.startHashEntry->orientation))
    {
        printf("Start Hash Entry is invalid after adding obstacles.\n");
        exit(1);
    }

    for (int i = 0; i < EnvROBARMCfg.nEndEffGoals; i++)
    {
        if(EnvROBARMCfg.Grid3D[EnvROBARMCfg.EndEffGoals_c[i][0]][EnvROBARMCfg.EndEffGoals_c[i][1]][EnvROBARMCfg.EndEffGoals_c[i][2]] >= EnvROBARMCfg.ObstacleCost)
        {
            printf("End Effector Goal(%u %u %u) is invalid after adding obstacles.\n",
                   EnvROBARMCfg.EndEffGoals_c[i][0],EnvROBARMCfg.EndEffGoals_c[i][1],EnvROBARMCfg.EndEffGoals_c[i][2]);
            exit(1);
        }
    }
}

void EnvironmentROBARM3D::ClearEnv()
{
    int x, y, z;

    // set all cells to zero
    for (x = 0; x < EnvROBARMCfg.EnvWidth_c; x++)
    {
        for (y = 0; y < EnvROBARMCfg.EnvHeight_c; y++)
        {
            for (z = 0; z < EnvROBARMCfg.EnvDepth_c; z++)
                EnvROBARMCfg.Grid3D[x][y][z] = 0;
        }
    }

    // set all cells to zero in lowres grid
    if(EnvROBARMCfg.lowres_collision_checking)
    {
        for (x = 0; x < EnvROBARMCfg.LowResEnvWidth_c; x++)
        {
            for (y = 0; y < EnvROBARMCfg.LowResEnvHeight_c; y++)
            {
                for (z = 0; z < EnvROBARMCfg.LowResEnvDepth_c; z++)
                    EnvROBARMCfg.LowResGrid3D[x][y][z] = 0;
            }
        }
    }
}

bool EnvironmentROBARM3D::isPathValid(double** path, int num_waypoints)
{
    double angles[NUMOFLINKS], orientation[3][3];
    short unsigned int endeff[3], wrist[3], elbow[3], coord[NUMOFLINKS];

    //loop through each waypoint of the path and check if it's valid
    for(int i = 0; i < num_waypoints; i++)
    {
        //compute FK
        ComputeEndEffectorPos(angles, endeff, wrist, elbow, orientation);

        for(int k = 0; k < NUMOFLINKS; k++)
        {
            angles[k] = path[i][k]*(180.0/PI_CONST);

            if (angles[k] < 0)
                angles[k] += 360;
        }

        //compute coords
        ComputeCoord(angles, coord);

        //check if valid
        if(!IsValidCoord(coord,endeff, wrist, elbow, orientation))
        {
            printf("[isPathValid] Path is invalid.\n");
            return false;
        }
    }

    printf("Path is valid.\n");
    return true;
}

  /**@brief Get the matrix represented as euler angles around ZYX
 * @param yaw Yaw around X axis
 * @param pitch Pitch around Y axis
 * @param roll around X axis
   * @param solution_number Which solution of two possible solutions ( 1 or 2) are possible values*/   
 /* void getEulerZYX(btScalar& yaw, btScalar& pitch, btScalar& roll, unsigned int solution_number = 1) const
{
    struct Euler{btScalar yaw, pitch, roll;};
    Euler euler_out;
    Euler euler_out2; //second solution
    //get the pointer to the raw data
   
    // Check that pitch is not at a singularity
    if (btFabs(m_el[2].x()) >= 1)
    {
        euler_out.yaw = 0;
        euler_out2.yaw = 0;
   
      // From difference of angles formula
        btScalar delta = btAtan2(m_el[0].x(),m_el[0].z());
        if (m_el[2].x() > 0)  //gimbal locked up
        {
            euler_out.pitch = SIMD_PI / btScalar(2.0);
            euler_out2.pitch = SIMD_PI / btScalar(2.0);
            euler_out.roll = euler_out.pitch + delta;
            euler_out2.roll = euler_out.pitch + delta;
        }
        else // gimbal locked down
        {
            euler_out.pitch = -SIMD_PI / btScalar(2.0);
            euler_out2.pitch = -SIMD_PI / btScalar(2.0);
            euler_out.roll = -euler_out.pitch + delta;
            euler_out2.roll = -euler_out.pitch + delta;
        }
    }
    else
    {
        euler_out.pitch = - btAsin(m_el[2].x());
        euler_out2.pitch = SIMD_PI - euler_out.pitch;
   
        euler_out.roll = btAtan2(m_el[2].y()/btCos(euler_out.pitch),
                                 m_el[2].z()/btCos(euler_out.pitch));
        euler_out2.roll = btAtan2(m_el[2].y()/btCos(euler_out2.pitch),
                                  m_el[2].z()/btCos(euler_out2.pitch));
   
        euler_out.yaw = btAtan2(m_el[1].x()/btCos(euler_out.pitch),
                                m_el[0].x()/btCos(euler_out.pitch));
        euler_out2.yaw = btAtan2(m_el[1].x()/btCos(euler_out2.pitch),
                                 m_el[0].x()/btCos(euler_out2.pitch));
    }
   
    if (solution_number == 1)
    {
        yaw = euler_out.yaw;
        pitch = euler_out.pitch;
        roll = euler_out.roll;
    }
    else
    {
        yaw = euler_out2.yaw;
        pitch = euler_out2.pitch;
        roll = euler_out2.roll;
    }
}
*/

void EnvironmentROBARM3D::getRPY(double Rot[3][3], double* roll, double* pitch, double* yaw)
{
    double delta;

     // Check that pitch is not at a singularity
    if(fabs(Rot[0][2]) >= 1)
    {
        *yaw  = 0;

        delta = atan2(Rot[0][0], Rot[2][0]);
        if(Rot[0][2] > 0)   //gimbal locked up
        {
            *pitch = PI_CONST / 2.0;
            *roll = *pitch + delta;
        }
        else
        {
            *pitch = -PI_CONST / 2.0;
            *roll = -*pitch + delta;
        }
    }
    else
    {
        *pitch = -asin(Rot[0][2]);

        *roll = atan2(Rot[1][2]/cos(*pitch),
                     Rot[2][2]/cos(*pitch));

        *yaw = atan2(Rot[0][1]/cos(*pitch),
                     Rot[0][0]/cos(*pitch));
    }
}
//--------------------------------------------------------------


/*------------------------------------------------------------------------*/
                        /* Printing Routines */
/*------------------------------------------------------------------------*/
void EnvironmentROBARM3D::PrintHeurGrid()
{
    int x,y,z;	
    int width = EnvROBARMCfg.EnvWidth_c;
    int height = EnvROBARMCfg.EnvHeight_c;
    int depth = EnvROBARMCfg.EnvDepth_c;
    if(EnvROBARMCfg.lowres_collision_checking)
    {
        width = EnvROBARMCfg.LowResEnvWidth_c;
        height = EnvROBARMCfg.LowResEnvHeight_c;
        depth =  EnvROBARMCfg.LowResEnvDepth_c;
    }

    for (x = 0; x < width; x++)
    {
        printf("\nx = %i\n",x);
        for (y = 0; y < height; y++)
        {
            for(z = 0; z < depth; z++)
            {
                printf("%3.0u ",EnvROBARM.Heur[XYZTO3DIND(x,y,z)]/COSTMULT);
            }
            printf("\n");
        }
        printf("\n");
    }
}

void EnvironmentROBARM3D::printangles(FILE* fOut, short unsigned int* coord, bool bGoal, bool bVerbose, bool bLocal)
{
    double angles[NUMOFLINKS];
    int dangles[NUMOFLINKS];
    int i;
    short unsigned int endeff[3];

    ComputeContAngles(coord, angles);

    if(bVerbose)
    {
        for (i = 0; i < NUMOFLINKS; i++)
        {
            dangles[i] = angles[i]*(180/PI_CONST) + 0.999999;
        }
    }

    if(bVerbose)
    	fprintf(fOut, "angles: ");
    for(i = 0; i < NUMOFLINKS; i++)
    {
        if(!bLocal)
            fprintf(fOut, "%i ", dangles[i]);
        else
        {
            if(i > 0)
                fprintf(fOut, "%i ", dangles[i]-dangles[i-1]);
            else
                fprintf(fOut, "%i ", dangles[i]);
        }
    }
// 	for(i = 0; i < NUMOFLINKS; i++)
// 	{
//         if(!bLocal)
//     		fprintf(fOut, "%1.2f ", angles[i]);
//         else
//         {
// 		    if(i > 0)
// 			    fprintf(fOut, "%1.2f ", angles[i]-angles[i-1]);
// 		    else
// 			    fprintf(fOut, "%1.2f ", angles[i]);
//         }
// 	}

    ComputeEndEffectorPos(angles, endeff);
    if(bGoal)
    {
        endeff[0] = EnvROBARM.goalHashEntry->endeff[0];
        endeff[1] = EnvROBARM.goalHashEntry->endeff[1];
        endeff[2] = EnvROBARM.goalHashEntry->endeff[2];
    }
    if(bVerbose)
    	fprintf(fOut, "   endeff: %d %d %d", endeff[0],endeff[1],endeff[2]);
    else
    	fprintf(fOut, "%d %d %d", endeff[0],endeff[1],endeff[2]);

    fprintf(fOut, "\n");
}

void EnvironmentROBARM3D::PrintAnglesWithAction(FILE* fOut, EnvROBARMHashEntry_t* HashEntry, bool bGoal, bool bVerbose, bool bLocal)
{
    double angles[NUMOFLINKS];
    int dangles[NUMOFLINKS];
    int i;

    //convert to angles
    ComputeContAngles(HashEntry->coord, angles);

    //convert to degrees
    if(bVerbose)
    {
        for (i = 0; i < NUMOFLINKS; i++)
        {
            dangles[i] = angles[i]*(180.0/PI_CONST) + 0.999999;
        }
    }

    if(bVerbose)
    	fprintf(fOut, "angles: ");

#if OUPUT_DEGREES
    for(i = 0; i < NUMOFLINKS; i++)
    {
        if(!bLocal)
            fprintf(fOut, "%-3i ", dangles[i]);
        else
        {
            if(i > 0)
                fprintf(fOut, "%-3i ", dangles[i]-dangles[i-1]);
            else
                fprintf(fOut, "%-3i ", dangles[i]);
        }
    }
#else
    for(i = 0; i < NUMOFLINKS; i++)
    {
        if(!bLocal)
            fprintf(fOut, "%-.3f ", angles[i]);
        else
        {
            if(i > 0)
                fprintf(fOut, "%-.3f ", angles[i]-angles[i-1]);
            else
                fprintf(fOut, "%-.3f ", angles[i]);
        }
    }
#endif

    if(bVerbose)
    	fprintf(fOut, "  endeff: %-2d %-2d %-2d   action: %d", HashEntry->endeff[0],HashEntry->endeff[1],HashEntry->endeff[2], HashEntry->action);
    else
    	fprintf(fOut, "%-2d %-2d %-2d %-2d", HashEntry->endeff[0],HashEntry->endeff[1],HashEntry->endeff[2],HashEntry->action);

    fprintf(fOut, "\n");

//     printf("[PrintAnglesWithAction] Exiting...\n");
}

/*
void EnvironmentROBARM3D::PrintAnglesWithAction(FILE* fOut, EnvROBARMHashEntry_t* HashEntry, bool bGoal, bool bVerbose, bool bLocal)
{
    double angles[NUMOFLINKS];
    int dangles[NUMOFLINKS];
    int i;

    if(bGoal)
        ComputeContAngles(EnvROBARMCfg.goalcoords, angles);
    else
        ComputeContAngles(HashEntry->coord, angles);

    if(bVerbose)
    {
        for (i = 0; i < NUMOFLINKS; i++)
        {
            dangles[i] = angles[i]*(180/PI_CONST) + 0.999999;
        }
    }

    if(bVerbose)
        fprintf(fOut, "angles: ");

#if OUPUT_DEGREES
    for(i = 0; i < NUMOFLINKS; i++)
    {
        if(!bLocal)
            fprintf(fOut, "%-3i ", dangles[i]);
        else
        {
            if(i > 0)
                fprintf(fOut, "%-3i ", dangles[i]-dangles[i-1]);
            else
                fprintf(fOut, "%-3i ", dangles[i]);
        }
    }
#else
    for(i = 0; i < NUMOFLINKS; i++)
    {
        if(!bLocal)
            fprintf(fOut, "%-.3f ", angles[i]);
        else
        {
            if(i > 0)
                fprintf(fOut, "%-.3f ", angles[i]-angles[i-1]);
            else
                fprintf(fOut, "%-.3f ", angles[i]);
        }
    }
#endif

    if(bGoal)
    {
        HashEntry->endeff[0] = EnvROBARMCfg.EndEffGoalX_c;
        HashEntry->endeff[1] = EnvROBARMCfg.EndEffGoalY_c;
        HashEntry->endeff[2] = EnvROBARMCfg.EndEffGoalZ_c;
    }
    if(bVerbose)
        fprintf(fOut, "  endeff: %-2d %-2d %-2d   action: %d", HashEntry->endeff[0],HashEntry->endeff[1],HashEntry->endeff[2], HashEntry->action);
    else
        fprintf(fOut, "%-2d %-2d %-2d %-2d", HashEntry->endeff[0],HashEntry->endeff[1],HashEntry->endeff[2],HashEntry->action);

    fprintf(fOut, "\n");
}
*/

void EnvironmentROBARM3D::PrintConfiguration()
{
    int i;
    double pX, pY, pZ;
    double start_angles[NUMOFLINKS], orientation[3][3];
    short unsigned int wrist[3],elbow[3],endeff[3];

    printf("\nEnvironment/Robot Details:\n");
    printf("Grid Cell Width: %.2f cm\n",EnvROBARMCfg.GridCellWidth*100);

    Cell2ContXYZ(EnvROBARMCfg.BaseX_c,EnvROBARMCfg.BaseY_c, EnvROBARMCfg.BaseZ_c,&pX, &pY, &pZ);
    printf("Shoulder Base: %i %i %i (cells) --> %.3f %.3f %.3f (meters)\n",EnvROBARMCfg.BaseX_c,EnvROBARMCfg.BaseY_c, EnvROBARMCfg.BaseZ_c,pX,pY,pZ);

    for(i = 0; i < NUMOFLINKS; i++)
        start_angles[i] = DEG2RAD(EnvROBARMCfg.LinkStartAngles_d[i]);

    ComputeEndEffectorPos(start_angles, endeff, wrist, elbow,orientation);
    Cell2ContXYZ(elbow[0],elbow[1],elbow[2],&pX, &pY, &pZ);
    printf("Elbow Start:   %i %i %i (cells) --> %.3f %.3f %.3f (meters)\n",elbow[0],elbow[1],elbow[2],pX,pY,pZ);

    Cell2ContXYZ(wrist[0],wrist[1],wrist[2],&pX, &pY, &pZ);
    printf("Wrist Start:   %i %i %i (cells) --> %.3f %.3f %.3f (meters)\n",wrist[0],wrist[1],wrist[2],pX,pY,pZ);

    Cell2ContXYZ(endeff[0],endeff[1],endeff[2], &pX, &pY, &pZ);
    printf("End Effector Start: %i %i %i (cells) --> %.3f %.3f %.3f (meters)\n",endeff[0],endeff[1],endeff[2],pX,pY,pZ);

    if(EnvROBARMCfg.bGoalIsSet)
    {
        for(i = 0; i < EnvROBARMCfg.nEndEffGoals; i++)
            printf("End Effector Goal:  %i %i %i (cells) --> %.3f %.3f %.3f (meters)\n",EnvROBARMCfg.EndEffGoals_c[i][0], EnvROBARMCfg.EndEffGoals_c[i][1],
                EnvROBARMCfg.EndEffGoals_c[i][2],EnvROBARMCfg.EndEffGoals_m[i][0],EnvROBARMCfg.EndEffGoals_m[i][1],EnvROBARMCfg.EndEffGoals_m[i][2]);
    }

#if OUTPUT_OBSTACLES
    int x, y, z;
    int sum = 0;
    printf("\nObstacles:\n");
    for (y = 0; y < EnvROBARMCfg.EnvHeight_c; y++)
	for (x = 0; x < EnvROBARMCfg.EnvWidth_c; x++)
        {
	    for (z = 0; z < EnvROBARMCfg.EnvDepth_c; z++)
	    {
		sum += EnvROBARMCfg.Grid3D[x][y][z];
                if (EnvROBARMCfg.Grid3D[x][y][z] >= EnvROBARMCfg.ObstacleCost)
		    printf("(%i,%i,%i) ",x,y,z);
	    }
        }
    printf("\nThe occupancy grid contains %i obstacle cells.\n",sum); 
#endif

    printf("\nDH Parameters:\n");
    printf("LinkTwist: ");
    for(i=0; i < NUMOFLINKS_DH; i++) 
        printf("%.2f  ",EnvROBARMCfg.DH_alpha[i]);
    printf("\nLinkLength: ");
    for(i=0; i < NUMOFLINKS_DH; i++) 
        printf("%.2f  ",EnvROBARMCfg.DH_a[i]);
    printf("\nLinkOffset: ");
    for(i=0; i < NUMOFLINKS_DH; i++) 
        printf("%.2f  ",EnvROBARMCfg.DH_d[i]);
    printf("\nJointAngles: ");
    for(i=0; i < NUMOFLINKS_DH; i++) 
        printf("%.2f  ",EnvROBARMCfg.DH_theta[i]);

    printf("\n\nMotor Limits:\n");
    printf("PosMotorLimits: ");
    for(i=0; i < NUMOFLINKS; i++) 
        printf("%1.2f  ",EnvROBARMCfg.PosMotorLimits[i]);
    printf("\nNegMotorLimits: ");
    for(i=0; i < NUMOFLINKS; i++) 
        printf("%1.2f  ",EnvROBARMCfg.NegMotorLimits[i]);
    printf("\n\n");
}

void EnvironmentROBARM3D::PrintAbridgedConfiguration()
{
    double pX, pY, pZ;
    double start_angles[NUMOFLINKS];
    short unsigned int endeff[3];

    ComputeEndEffectorPos(start_angles, endeff);
    Cell2ContXYZ(endeff[0],endeff[1],endeff[2], &pX, &pY, &pZ);
    printf("End Effector Start: %i %i %i (cells) --> %.3f %.3f %.3f (meters)\n",endeff[0],endeff[1],endeff[2],pX,pY,pZ);

    for(int i = 0; i < EnvROBARMCfg.nEndEffGoals; i++)
        printf("End Effector Goal:  %i %i %i (cells) --> %.3f %.3f %.3f (meters)\n",EnvROBARMCfg.EndEffGoals_c[i][0], EnvROBARMCfg.EndEffGoals_c[i][1],
               EnvROBARMCfg.EndEffGoals_c[i][2],EnvROBARMCfg.EndEffGoals_m[i][0],EnvROBARMCfg.EndEffGoals_m[i][1],EnvROBARMCfg.EndEffGoals_m[i][2]);

    printf("\n");
}

//display action cost table
void EnvironmentROBARM3D::OutputActionCostTable()
{
    int x,y;
    printf("\nAction Cost Table\n");
    for (x = 0; x < EnvROBARMCfg.nSuccActions; x++)
    {
        printf("%i: ",x); 
        for (y = 0; y < EnvROBARMCfg.nSuccActions; y++)
            printf("%i  ",EnvROBARMCfg.ActiontoActionCosts[x][y]);
        printf("\n");
    }
    printf("\n");
}

//display successor actions
void EnvironmentROBARM3D::OutputActions()
{
    int x,y;
    printf("\nSuccessor Actions\n");
    for (x=0; x < EnvROBARMCfg.nSuccActions; x++)
    {
        printf("%i:  ",x);
        for(y=0; y < NUMOFLINKS; y++)
            printf("%.1f  ",EnvROBARMCfg.SuccActions[x][y]);

        printf("\n");
    }
}

void EnvironmentROBARM3D::OutputPlanningStats()
{
    printf("\n# Possible Actions: %d\n",EnvROBARMCfg.nSuccActions);
//     printf("# GetHashEntry Calls: %i  computed in %3.2f sec\n",num_GetHashEntry,time_gethash/(double)CLOCKS_PER_SEC); 
    printf("# Forward Kinematic Computations: %i  Time per Computation: %.3f (usec)\n",num_forwardkinematics,(DH_time/(double)CLOCKS_PER_SEC)*1000000 / num_forwardkinematics);
    printf("Cost per Cell: %.2f   *sqrt(2): %.2f   *sqrt(3):  %.2f\n",EnvROBARMCfg.cost_per_cell,EnvROBARMCfg.cost_sqrt2_move,EnvROBARMCfg.cost_sqrt3_move);

    if(EnvROBARMCfg.use_DH)
        printf("DH Convention: %3.2f sec\n", DH_time/(double)CLOCKS_PER_SEC);
    else
        printf("Kinematics Library: %3.2f sec\n", KL_time/(double)CLOCKS_PER_SEC);

    printf("Collision Checking: %3.2f sec\n", check_collision_time/(double)CLOCKS_PER_SEC);
    printf("\n");
}
//--------------------------------------------------------------


/*------------------------------------------------------------------------*/
                        /* Forward Kinematics */
/*------------------------------------------------------------------------*/
void EnvironmentROBARM3D::InitializeKinNode() //needed when using kinematic library
{
    clock_t currenttime = clock();

    char *c_filename = getenv("ROS_PACKAGE_PATH");
    std::stringstream filename;
    filename << c_filename << "/robot_descriptions/wg_robot_description/pr2/pr2.xml" ;
    EnvROBARMCfg.pr2_kin.loadXML(filename.str());

    EnvROBARMCfg.left_arm = EnvROBARMCfg.pr2_kin.getSerialChain("right_arm");
    assert(EnvROBARMCfg.left_arm);    
    EnvROBARMCfg.pr2_config = new JntArray(EnvROBARMCfg.left_arm->num_joints_);

//     printf("Node initialized.\n");
    /*std::string pr2Content;
    calcFK_armplanner.getParam("robotdesc/pr2",pr2Content);  
    EnvROBARMCfg.pr2_kin.loadString(pr2Content.c_str());*/

    KL_time += clock() - currenttime;
}

void EnvironmentROBARM3D::CloseKinNode()
{
//     ros::fini();
    sleep(1);
}

//pre-compute DH matrices for all 8 frames with NUM_TRANS_MATRICES degree resolution 
void EnvironmentROBARM3D::ComputeDHTransformations()
{
    int i,n,theta;
    double c_alpha,s_alpha;

    //precompute cos and sin tables
    for (i = 0; i < 360; i++)
    {
        EnvROBARMCfg.cos_r[i] = cos(DEG2RAD(i));
        EnvROBARMCfg.sin_r[i] = sin(DEG2RAD(i));
    }

    // assign transformation matrices 
    for (n = 0; n < NUMOFLINKS_DH; n++)
    {
        c_alpha = cos(EnvROBARMCfg.DH_alpha[n]);
        s_alpha = sin(EnvROBARMCfg.DH_alpha[n]);

        theta=0;
        for (i = 0; i < NUM_TRANS_MATRICES*4; i+=4)
        {
            EnvROBARMCfg.T_DH[i][0][n] = EnvROBARMCfg.cos_r[theta];
            EnvROBARMCfg.T_DH[i][1][n] = -EnvROBARMCfg.sin_r[theta]*c_alpha; 
            EnvROBARMCfg.T_DH[i][2][n] = EnvROBARMCfg.sin_r[theta]*s_alpha;
            EnvROBARMCfg.T_DH[i][3][n] = EnvROBARMCfg.DH_a[n]*EnvROBARMCfg.cos_r[theta];

            EnvROBARMCfg.T_DH[i+1][0][n] = EnvROBARMCfg.sin_r[theta];
            EnvROBARMCfg.T_DH[i+1][1][n] = EnvROBARMCfg.cos_r[theta]*c_alpha;
            EnvROBARMCfg.T_DH[i+1][2][n] = -EnvROBARMCfg.cos_r[theta]*s_alpha;
            EnvROBARMCfg.T_DH[i+1][3][n] = EnvROBARMCfg.DH_a[n]*EnvROBARMCfg.sin_r[theta];


            EnvROBARMCfg.T_DH[i+2][0][n] = 0.0;
            EnvROBARMCfg.T_DH[i+2][1][n] = s_alpha;
            EnvROBARMCfg.T_DH[i+2][2][n] = c_alpha;
            EnvROBARMCfg.T_DH[i+2][3][n] = EnvROBARMCfg.DH_d[n];

            EnvROBARMCfg.T_DH[i+3][0][n] = 0.0;
            EnvROBARMCfg.T_DH[i+3][1][n] = 0.0;
            EnvROBARMCfg.T_DH[i+3][2][n] = 0.0;
            EnvROBARMCfg.T_DH[i+3][3][n] = 1.0;

            theta++;
        }
    }
}

//uses DH convention
void EnvironmentROBARM3D::ComputeForwardKinematics_DH(double angles[NUMOFLINKS])
{
    double sum, theta[NUMOFLINKS_DH];
    int t,x,y,k;

    theta[0] = EnvROBARMCfg.DH_theta[0];
    theta[1] = angles[0] + EnvROBARMCfg.DH_theta[1];
    theta[2] = angles[1] + EnvROBARMCfg.DH_theta[2];
    theta[3] = angles[2] + EnvROBARMCfg.DH_theta[3];
    theta[4] = angles[3] + EnvROBARMCfg.DH_theta[4];
    theta[5] = -angles[4] + EnvROBARMCfg.DH_theta[5];
    theta[6] = -angles[5] + EnvROBARMCfg.DH_theta[6];
    theta[7] = angles[6] + EnvROBARMCfg.DH_theta[7];

    // compute transformation matrices and premultiply
    for(int i = 0; i < NUMOFLINKS_DH; i++)
    {
        //convert theta to degrees
        theta[i]=theta[i]*(180.0/PI_CONST);

        //make sure theta is not negative
        if (theta[i] < 0)
            theta[i] = 360.0+theta[i];

        //round to nearest integer
        t = theta[i] + 0.5;

        //multiply by 4 to get to correct position in T_DH
        t = t*4;

        //multiply by previous transformations to put in shoulder frame
        if(i > 0)
        {
            for (x=0; x<4; x++)
            {
                for (y=0; y<4; y++)
                {
                    sum = 0;
                    for (k=0; k<4; k++)
                        sum += EnvROBARM.Trans[x][k][i-1] * EnvROBARMCfg.T_DH[t+k][y][i];

                    EnvROBARM.Trans[x][y][i] = sum;
                }
            }
        }
        else
        {
            for (x=0; x<4; x++)
                for (y=0; y<4; y++)
                    EnvROBARM.Trans[x][y][0] = EnvROBARMCfg.T_DH[t+x][y][0];
        }
    }
}

//uses ros's KL Library
void EnvironmentROBARM3D::ComputeForwardKinematics_ROS(double *angles, int f_num, double *x, double *y, double*z)
{
    Frame f, f2;
    KDL::Vector gripper(0.0, 0.0, EnvROBARMCfg.DH_d[NUMOFLINKS_DH-1]);

    for(int i = 0; i < NUMOFLINKS; i++)
        (*EnvROBARMCfg.pr2_config)(i) = angles[i];

    EnvROBARMCfg.left_arm->computeFK((*EnvROBARMCfg.pr2_config),f,f_num);

    //add translation from wrist to end of fingers
    if(f_num == 7)
    {
        f.p = f.M*gripper + f.p;

        for(int i = 0; i < 3; i++)
            for(int j = 0; j < 3; j++)
                EnvROBARM.Trans[i][j][7] = f.M(i,j);
    }

    //translate xyz coordinates from shoulder frame to base frame
    *x = f.p[0] + EnvROBARMCfg.BaseX_m;
    *y = f.p[1] + EnvROBARMCfg.BaseY_m;
    *z = f.p[2] + EnvROBARMCfg.BaseZ_m;
}

//pre-compute action costs
void EnvironmentROBARM3D::ComputeActionCosts()
{

    int i,x,y;
    double temp = 0.0;
    EnvROBARMCfg.ActiontoActionCosts = new int* [EnvROBARMCfg.nSuccActions];
    for (x = 0; x < EnvROBARMCfg.nSuccActions; x++)
    {
        EnvROBARMCfg.ActiontoActionCosts[x] = new int[EnvROBARMCfg.nSuccActions];
        for (y = 0; y < EnvROBARMCfg.nSuccActions; y++)
        {
            temp = 0.0;
            for (i = 0; i < NUMOFLINKS; i++)
            {
                temp += ((EnvROBARMCfg.SuccActions[x][i]-EnvROBARMCfg.SuccActions[y][i])*(EnvROBARMCfg.SuccActions[x][i]-EnvROBARMCfg.SuccActions[y][i]));
            }
            EnvROBARMCfg.ActiontoActionCosts[x][y] = temp * COSTMULT * EnvROBARMCfg.smoothing_weight;
        }
    }

/*
    int i,x,y;
    double temp = 0.0;
    EnvROBARMCfg.LowResActionCosts = new int* [EnvROBARMCfg.nLowResActions];
    for (x = 0; x < EnvROBARMCfg.nLowResActions; x++)
    {
        EnvROBARMCfg.LowResActionCosts[x] = new int[EnvROBARMCfg.nLowResActions];
        for (y = 0; y < EnvROBARMCfg.nLowResActions; y++)
        {
            temp = 0.0;
            for (i = 0; i < NUMOFLINKS; i++)
            {
                temp += ((EnvROBARMCfg.LowResActions[x][i]-EnvROBARMCfg.LowResActions[y][i])*(EnvROBARMCfg.LowResActions[x][i]-EnvROBARMCfg.LowResActions[y][i]));
            }
            EnvROBARMCfg.LowResActionCosts[x][y] = temp * COSTMULT * EnvROBARMCfg.smoothing_weight;
        }
    }

    EnvROBARMCfg.HighResActionCosts = new int* [EnvROBARMCfg.nHighResActions];
    for (x = 0; x < EnvROBARMCfg.nHighResActions; x++)
    {
        EnvROBARMCfg.HighResActionCosts[x] = new int[EnvROBARMCfg.nHighResActions];
        for (y = 0; y < EnvROBARMCfg.nHighResActions; y++)
        {
            temp = 0.0;
            for (i = 0; i < NUMOFLINKS; i++)
            {
                temp += ((EnvROBARMCfg.HighResActions[x][i]-EnvROBARMCfg.HighResActions[y][i])*(EnvROBARMCfg.HighResActions[x][i]-EnvROBARMCfg.HighResActions[y][i]));
            }
            EnvROBARMCfg.HighResActionCosts[x][y] = temp * COSTMULT * EnvROBARMCfg.smoothing_weight;
        }
    }
*/
}

void EnvironmentROBARM3D::ComputeCostPerCell()
{
    double angles[NUMOFLINKS],start_angles[NUMOFLINKS]={0};
    double eucl_dist, max_dist = 0;
    int largest_action=0;;
    double start_endeff_m[3], endeff_m[3];

    //starting at zeroed angles, find end effector position after each action
    if(EnvROBARMCfg.use_DH)
        ComputeEndEffectorPos(start_angles,start_endeff_m);
    else
        ComputeForwardKinematics_ROS(start_angles, 7, &(start_endeff_m[0]), &(start_endeff_m[1]), &(start_endeff_m[2]));

    //iterate through all possible actions and find the one with the minimum cost per cell
    for (int i = 0; i < EnvROBARMCfg.nSuccActions; i++)
    {

        for(int j = 0; j < NUMOFLINKS; j++)
            angles[j] = DEG2RAD(EnvROBARMCfg.SuccActions[i][j]);// * (360.0/ANGLEDELTA));

        //starting at zeroed angles, find end effector position after each action
        if(EnvROBARMCfg.use_DH)
            ComputeEndEffectorPos(angles,endeff_m);
        else
            ComputeForwardKinematics_ROS(angles, 7, &(endeff_m[0]), &(endeff_m[1]), &(endeff_m[2]));

        eucl_dist = sqrt((start_endeff_m[0]-endeff_m[0])*(start_endeff_m[0]-endeff_m[0]) + 
                        (start_endeff_m[1]-endeff_m[1])*(start_endeff_m[1]-endeff_m[1]) + 
                        (start_endeff_m[2]-endeff_m[2])*(start_endeff_m[2]-endeff_m[2]));

        if (eucl_dist > max_dist)
        {
            max_dist = eucl_dist;
            largest_action = i;
        }
    }

    if(EnvROBARMCfg.lowres_collision_checking)
        EnvROBARMCfg.CellsPerAction = max_dist/EnvROBARMCfg.LowResGridCellWidth;
    else
        EnvROBARMCfg.CellsPerAction = max_dist/EnvROBARMCfg.GridCellWidth;
    
    EnvROBARMCfg.cost_per_cell = COSTMULT/EnvROBARMCfg.CellsPerAction;
    EnvROBARMCfg.cost_sqrt2_move = sqrt(2.0)*EnvROBARMCfg.cost_per_cell;
    EnvROBARMCfg.cost_sqrt3_move = sqrt(3.0)*EnvROBARMCfg.cost_per_cell;
}

//just used to compare the KL with DH convention
//use this function to compare link lengths
/*
void EnvironmentROBARM3D::ValidateDH2KinematicsLibrary() //very very hackish
{
    double angles[NUMOFLINKS] = {0, .314, 0, -0.7853, 0, 0, 0};
    short unsigned int wrist1[3],wrist2[3];
    short unsigned int elbow1[3],elbow2[3];
    short unsigned int endeff1[3],endeff2[3];

    //compare DH vs KL
    for (double i = -1; i < 1; i+=.005)
    {
        //choose random values
        angles[0] = i*2;
        angles[1] += i;
        angles[2] = 0;
        angles[3] = 0;
        angles[4] = i/3;
        angles[5] = -i/8;
        angles[6] = i/5;

        //DH
        EnvROBARMCfg.use_DH = 1;
        ComputeEndEffectorPos(angles, endeff1, wrist1, elbow1);
        //KL
        EnvROBARMCfg.use_DH = 0;
        ComputeEndEffectorPos(angles, endeff2, wrist2, elbow2);

        if(endeff1[0] != endeff2[0] || endeff1[1] != endeff2[1] || endeff1[2] != endeff2[2] || 
            wrist1[0] != wrist2[0] || wrist1[1] != wrist2[1] || wrist1[2] != wrist2[2] ||
            elbow1[0] != elbow2[0] || elbow1[1] != elbow2[1] || elbow1[2] != elbow2[2])
        {
            printf("ERROR -->  ");
            printf("DH: %i,%i,%i   KL: %i,%i,%i  angles: ",endeff1[0],endeff1[1],endeff1[2],endeff2[0],endeff2[1],endeff2[2]);
            for (int j = 0; j < 7; j++)
                printf("%.3f  ",angles[j]);
            printf("\n");
            printf("      elbow --> DH: %i,%i,%i   KL: %i,%i,%i\n",elbow1[0],elbow1[1],elbow1[2],elbow2[0],elbow2[1],elbow2[2]);
            printf("      wrist --> DH: %i,%i,%i   KL: %i,%i,%i\n",wrist1[0],wrist1[1],wrist1[2],wrist2[0],wrist2[1],wrist2[2]);
        }
        else
            printf("DH: %i,%i,%i   KL: %i,%i,%i\n", endeff1[0],endeff1[1],endeff1[2],endeff2[0],endeff2[1],endeff2[2]);
    }
}
*/

int EnvironmentROBARM3D::GetEuclideanDistToGoal(short unsigned int* xyz)
{
    return sqrt((EnvROBARMCfg.EndEffGoalX_c - xyz[0])*(EnvROBARMCfg.EndEffGoalX_c - xyz[0]) +
                (EnvROBARMCfg.EndEffGoalY_c - xyz[1])*(EnvROBARMCfg.EndEffGoalY_c - xyz[1]) +
                (EnvROBARMCfg.EndEffGoalZ_c - xyz[2])*(EnvROBARMCfg.EndEffGoalZ_c - xyz[2]));
}
//--------------------------------------------------------------

/*------------------------------------------------------------------------*/
                        /* For Taking Statistics */
/*------------------------------------------------------------------------*/
bool EnvironmentROBARM3D::InitializeEnvForStats(const char* sEnvFile,  int n)
{
    // default values - temporary solution
    EnvROBARMCfg.use_DH = 1;
    EnvROBARMCfg.enforce_motor_limits = 1;
    EnvROBARMCfg.dijkstra_heuristic = 1;
    EnvROBARMCfg.endeff_check_only = 0;
    EnvROBARMCfg.padding = 0.06;
    EnvROBARMCfg.smoothing_weight = 0.0;
    EnvROBARMCfg.use_smooth_actions = 1;
    EnvROBARMCfg.gripper_orientation_moe =  .018; // 0.0125;
    EnvROBARMCfg.object_grasped = 0;
    EnvROBARMCfg.grasped_object_length_m = .1;
    EnvROBARMCfg.enforce_upright_gripper = 0;
    EnvROBARMCfg.goal_moe_m = .1;
    EnvROBARMCfg.JointSpaceGoal = 0;
//     EnvROBARMCfg.goal_moe_r = .15;
    EnvROBARMCfg.checkEndEffGoalOrientation = 0;

    //parse the parameter file - temporary
    char parFile[] = "params.cfg";
    FILE* fCfg = fopen(parFile, "r");
    if(fCfg == NULL)
	printf("ERROR: unable to open %s.....using defaults.\n", parFile);

    ReadParamsFile(fCfg);
    fclose(fCfg);

    //parse the configuration file
    fCfg = fopen(sEnvFile, "r");
    if(fCfg == NULL)
    {
	printf("ERROR: unable to open %s\n", sEnvFile);
	exit(1);
    }
    ReadConfiguration(fCfg);
    fclose(fCfg);

    //read in the starting positions of the joints and the end effector position
    char statsFile[] = "StatsFile.cfg";
    fCfg = fopen(statsFile, "r");
    if(fCfg == NULL)
    {
	printf("ERROR: unable to open %s\n", sEnvFile);
	exit(1);
    }
    InitializeStatistics(fCfg, n);

    //initialize forward kinematics
    if (!EnvROBARMCfg.use_DH)
        //start ros node
        InitializeKinNode();
    else
        //pre-compute DH Transformations
        ComputeDHTransformations();

    //initialize other parameters of the environment
    InitializeEnvConfig();

    //initialize Environment
    if(InitializeEnvironment() == false)
        return false;

    //pre-compute action-to-action costs
    ComputeActionCosts();

    //compute the cost per cell to be used by heuristic
    ComputeCostPerCell();

    //pre-compute heuristics
    ComputeHeuristicValues();

#if VERBOSE
    //output environment data
//     printf("Start:  ");
//     for (int k = 0; k < 7; k++)
//         printf("%.2f  ",EnvROBARMCfg.LinkStartAngles_d[k]);
//     printf("\n");

    PrintAbridgedConfiguration();
#endif
    return true;
}

void EnvironmentROBARM3D::InitializeStatistics(FILE* fCfg, int n)
{
    char sTemp[1024];
    double x,y,z;
//     int stop = 0;

//     while(!feof(fCfg) && strlen(sTemp) != 0)
//     {
    for(int j = 0; j < 10*(n-1); j++)
        fscanf(fCfg, "%s", sTemp);

    //parse starting joint angles
    for (int i = 0; i < NUMOFLINKS; i++)
    {
        fscanf(fCfg, "%s", sTemp);
        EnvROBARMCfg.LinkStartAngles_d[i] = 0;//atof(sTemp);
    }

    //end effector goal x
    fscanf(fCfg, "%s", sTemp);
    x = atof(sTemp);
    fscanf(fCfg, "%s", sTemp);
    y = atof(sTemp);
    fscanf(fCfg, "%s", sTemp);
    z = atof(sTemp);


    ContXYZ2Cell(x, y,z, &EnvROBARMCfg.EndEffGoalX_c, &EnvROBARMCfg.EndEffGoalY_c, &EnvROBARMCfg.EndEffGoalZ_c);
//     printf("Start: %i %i %i\n",EnvROBARMCfg.EndEffGoalX_c, EnvROBARMCfg.EndEffGoalY_c, EnvROBARMCfg.EndEffGoalZ_c);
//         if(stop == n)
//             break;
//         stop++;
//     }

//     if(stop != n)
//     {
//         printf("StatsFile does not contain the desired amount of lines.\n");
//         exit(1);
//     }
}

