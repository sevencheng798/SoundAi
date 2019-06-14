//  Created by kshlou@wingtech.com on 19-3-25.  #include<stdio.h> #include<stdlib.h> 
#ifndef _PROPERTIES_H__
#define _PROPERTIES_H__

#include<unistd.h> 
#include<string.h>   
#include<stdlib.h>   
#include<stdio.h>   

#ifdef __cplusplus  
extern "C"  {
#endif

#define GET_ARRAY_LEN(array,len) {len = (sizeof(array) / sizeof(array[0]));}
// According the key to get the value
void getprop(char* key,char* val); 
inline void  getprop(char* key,char* val) 
{
   char def_val[125]; 
   char tmp[125];  
   char str[20];
   char *ret=NULL;
   FILE *fp=NULL; 

   strcpy(def_val,val); 
   sprintf(str,"getprop %s",key);
   fp=popen(str,"r"); 
   ret = fgets(tmp, sizeof(tmp), fp);  
   if(ret !=NULL)
   {
	 tmp[strlen(tmp)-1] = 0;
	 strcpy(val,tmp); 
   }
   else
  	 strcpy(val,def_val);
   pclose(fp); 
}


//Modify the key's val,if no the key ,will add the key & val to default file
void setprop(char* key,char* val); 
inline void  setprop(char* key,char* val) 
{
   char str[20];
   sprintf(str,"setprop %s %s",key,val);
   system(str);
}

#ifdef __cplusplus  
}
#endif

#endif //
