#include <pebble.h>
#include <types.h>

void sort(struct TimeSegment* input_array[], int n) {
    int j,i;

    for(i=1;i<n;i++)
    {
        for(j=0;j<n-i;j++)
        {
            if(input_array[j]->value >input_array[j+1]->value)
            {
                struct TimeSegment *temp = input_array[j];
                input_array[j] =input_array[j+1];
                input_array[j+1] = temp;
            }
        }
    }
}