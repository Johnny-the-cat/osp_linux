/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: trash
 *
 * Created on May 6, 2020, 10:16 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include "oratypes.h"
#include "oratest.h"


int main(int argc, char** argv) 
{

    char primarylogin[32]={0};
    char primarypass[32]={0};
    char primarydblink[128]={0};
    char standbylogin[32]={0};
    char standbypass[32]={0};
    char standbydblink[128]={0};
    char pathparameter[256]={0};
    
    
    parseCmdLine(argc, argv, primarylogin, primarypass, primarydblink, standbylogin, standbypass, standbydblink, pathparameter);
    /*
    printf("primarylogin - %s\n", primarylogin);
    printf("primarypass - %s\n", primarypass);
    printf("primarydblink - %s\n", primarydblink);
    printf("standbylogin - %s\n", standbylogin);
    printf("standbypass - %s\n", standbypass);
    printf("standbydblink - %s\n", standbydblink);
    printf("path parameter - %s\n", pathparameter);
    */
     
    const char *error;
    void *ocimodule;


     // Load dynamically loaded library 
     ocimodule = dlopen("libclntsh.so", RTLD_LAZY);
     if (!ocimodule) 
     {
       fprintf(stderr, "Couldn't open libclntsh.so: %s\n", dlerror());
       exit(EXIT_FAILURE);
     }

    if (!LoadOciFunctions(ocimodule))
    {
        printf("Can't load OCI functions, OCI.dll is incompatible. Check that Oracle Client has been installed and PATH environment variable\n");
        dlclose(ocimodule);
        exit(EXIT_FAILURE);
    }

    if (!InitOraEnvironment())
    {
        printf("Can't Init OCI Environment for work, quiting...\n");
        dlclose(ocimodule);
        return EXIT_FAILURE;
    }

    printf("Connecting to standby server\n");
    if (!CreateStandbySession(standbylogin, standbypass, standbydblink, !strcmp("sys", standbylogin)))
    {
        printf("Can't create standby session, quiting...\n");
        CloseOraEnvironment();
        dlclose(ocimodule);
        return EXIT_FAILURE;
    }
    
    int filenumber;
    char standbyfilename [513];
    char standbyfinalfilename [513];
    char TBSName [32];
    int result = 0;
    
    result = GetStandbyUnnamedFile (&filenumber, standbyfilename, TBSName);
    if (result == FALSE)
    {
        printf ("Can not get standby files. Exit\n");
        CloseStandbySession();
        CloseOraEnvironment();
        dlclose(ocimodule);
        return EXIT_FAILURE;
    }
    else if (result == NO_UNNAMED_FILE)
    {
        printf ("There is no unnamed file on standby server, nothing to do\n");
        CloseStandbySession();
        CloseOraEnvironment();
        dlclose(ocimodule);
        return EXIT_SUCCESS;
    }
    
    //printf ("Unnamed file number is %d\n", filenumber);
    printf ("Standby filename is %s, number %d, %s tablespace\n", standbyfilename, filenumber, TBSName);
    //printf ("Tablespacename is %s\n", TBSName);
    
    //There! If pathparameter = gennext - generate name? create file and close Env
    //GetNextDatafileName(const char* tablespace, char* datafilename, unsigned int bufSizeDatafilename)
    if (strcmp(pathparameter, "gennext") == 0)
    {
        
        if(!GetNextDatafileName(TBSName, standbyfinalfilename, sizeof(standbyfinalfilename)))
        {
            printf ("Can not get next filename. Exit\n");
            CloseStandbySession();
            CloseOraEnvironment();
            dlclose(ocimodule);
            return EXIT_FAILURE;
        }
        
        printf ("Path parameter is \"gennext\", next filename is %s\n", standbyfinalfilename);
        
        if (!CreateStandbyDatafile(standbyfilename, standbyfinalfilename))
        {
            printf ("Error creating a datafile on standby server\n");
            CloseStandbySession();
            CloseOraEnvironment();
            dlclose(ocimodule);
            return EXIT_FAILURE;
        }
    
        printf ("Successfully complete\n");
        CloseStandbySession();
        CloseOraEnvironment(); 
        dlclose(ocimodule);
        return (EXIT_SUCCESS);
        
    }
    
    
    printf("Connecting to primary server\n");
    if (!CreatePrimarySession(primarylogin, primarypass, primarydblink, !strcmp("sys", primarylogin)))
    {
        printf("Can't create primary session, quiting...\n");
        CloseStandbySession();
        CloseOraEnvironment();
        dlclose(ocimodule);
        return EXIT_FAILURE;
    }
    
    char primaryfilename [513];
    if (!GetPrimaryFileByNumber (filenumber, primaryfilename))
    {
        printf("Can not get filename from primary server, quiting...\n");
        ClosePrimarySession();
        CloseStandbySession();
        CloseOraEnvironment();
        dlclose(ocimodule);
        return EXIT_FAILURE;
    }
    printf ("Primary filename is %s\n", primaryfilename);
    ClosePrimarySession();
    
    if (strcmp(pathparameter, "primarylike") == 0)
    {
        strncpy(standbyfinalfilename, primaryfilename, 513);
        printf ("Path parameter is \"primarylike\", the path for the datafile will remain the same as on the primary server - %s\n", standbyfinalfilename);
    }
    else if (strcmp(pathparameter, "auto") == 0)
    {
        printf ("Path parameter is \"auto\", the directory for the datafile will be the same as the last one in this table space\n");
        result = GetLastTBSFile(TBSName, standbyfinalfilename);
        if (result == FALSE)
        {
            printf("Error getting filename location, quiting...\n");
            CloseStandbySession();
            CloseOraEnvironment();
            dlclose(ocimodule);
            return EXIT_FAILURE;
        }
        else if (result == NO_NAMED_FILE)
        {
            printf ("There are no named files in this tablespace\n");
            CloseStandbySession();
            CloseOraEnvironment();
            dlclose(ocimodule);
            return EXIT_FAILURE;
        }
        
        int i;
        for (i = strlen(standbyfinalfilename); *(standbyfinalfilename + i) != '\\' &&  *(standbyfinalfilename + i) != '/'; i--);
        *(standbyfinalfilename + i + 1) = 0; 
        
        char * correctfilename;
        for (i = strlen(primaryfilename); *(primaryfilename + i) != '\\' &&  *(primaryfilename + i) != '/'; i--);
        correctfilename = primaryfilename + i + 1;
        
        strcat(standbyfinalfilename, correctfilename);
        printf ("Name for unnamed datafile is %s\n",standbyfinalfilename);
        
    }
    else if (strcmp(pathparameter, "autoforce") == 0)
    {
        printf ("Path parameter is \"autoforce\", the directory for the datafile will be the same as the last one in this tablespace.\nIf there is no tablespace yet, the directory for the datafile will be the same as the SYSTEM tablespace\n");
        result = GetLastTBSFile(TBSName, standbyfinalfilename);
        if (result == FALSE)
        {
            printf("Error getting filename location, quiting...\n");
            CloseStandbySession();
            CloseOraEnvironment();
            dlclose(ocimodule);
            return EXIT_FAILURE;
        }
        else if (result == NO_NAMED_FILE)
        {
            printf ("There are no named files in this tablespace, getting SYSTEM datafile location\n");
            if (GetLastTBSFile("SYSTEM", standbyfinalfilename) != NAMED_FILE_FOUND)
            {
                printf("Can not get SYSTEM filename location, quiting...\n");
                CloseStandbySession();
                CloseOraEnvironment();
                dlclose(ocimodule);
                return EXIT_FAILURE;
            }
        }
        //printf ("%s\n",standbyfinalfilename);
        int i;
        for (i = strlen(standbyfinalfilename); *(standbyfinalfilename + i) != '\\' &&  *(standbyfinalfilename + i) != '/'; i--);
        *(standbyfinalfilename + i + 1) = 0; 
        
        char * correctfilename;
        for (i = strlen(primaryfilename); *(primaryfilename + i) != '\\' &&  *(primaryfilename + i) != '/'; i--);
        correctfilename = primaryfilename + i + 1;
        
        strcat(standbyfinalfilename, correctfilename);
        printf ("Name for unnamed datafile is %s\n",standbyfinalfilename);
    }
    else
    {
        if (*pathparameter != '/')
        {
            printf ("It looks like you entered a relative path. Quiting to avoid problems\n");
            CloseStandbySession();
            CloseOraEnvironment();
            dlclose(ocimodule);
            return EXIT_FAILURE;
        }
        
        printf ("Path parameter is real path to datafile - %s\n", pathparameter);
        int i = strlen(pathparameter);
        char * correctfilename;
        
        if (pathparameter[i-1] != '/')
        {
            strncpy(standbyfinalfilename, pathparameter, 513);
            for (i = strlen(primaryfilename); *(primaryfilename + i) != '\\' &&  *(primaryfilename + i) != '/'; i--);
            correctfilename = primaryfilename + i ;
            strcat(standbyfinalfilename, correctfilename);
            printf ("Name for unnamed datafile is %s\n",standbyfinalfilename);
        }
        else
        {
            strncpy(standbyfinalfilename, pathparameter, 513);
            for (i = strlen(primaryfilename); *(primaryfilename + i) != '\\' &&  *(primaryfilename + i) != '/'; i--);
            correctfilename = primaryfilename + i + 1;
            strcat(standbyfinalfilename, correctfilename);
            printf ("Name for unnamed datafile is %s\n",standbyfinalfilename);
        }
        
    }
    
    if (!CreateStandbyDatafile(standbyfilename, standbyfinalfilename))
    {
        printf ("Error creating a datafile on standby server\n");
        CloseStandbySession();
        CloseOraEnvironment();
        dlclose(ocimodule);
        return EXIT_FAILURE;
    }
    
    printf ("Successfully complete\n");
    CloseStandbySession();
    CloseOraEnvironment();
     // All done, close things cleanly 
    dlclose(ocimodule);

    
    return (EXIT_SUCCESS);
}

