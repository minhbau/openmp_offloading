#include <cstdio>
#include <cstdlib>
#include <omp.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cuda_runtime.h>
#include <omp.h>
#include "tnsrtrns.h"


int main(int argc, char *argv[])
{

  if (argc<2)
  {
    fprintf(stderr,"Usage: taskgen <file>\n");
    return -1;
  }

  std::ifstream hfTask(argv[1]);

  size_t n_task;
  std::vector<tnsrtrns_task> task_list;
  std::string line;

  hfTask >> n_task;
  getline(hfTask,line); // skip to the next line

  std::cout << "Reading "<<n_task<<" tasks..."<<std::endl;

  int i_task=0;

  while (getline(hfTask,line))
  {
    std::stringstream ss_line(line);
    std::vector<int> line_int((std::istream_iterator<int>(ss_line)),
                              (std::istream_iterator<int>()) );
    if (line_int.size()<2) continue; // skip empty lines
    std::cout << std::setw(3) << i_task << " - " << line << std::endl;
    task_list.emplace_back(sizeof(double),line_int);
    i_task++;
  }
  hfTask.close();

  // Run the bigger tasks first for better load balancing
  std::vector<size_t> q(n_task);
  std::iota(q.begin(),q.end(),0); // must be 0-based, since C array is 0-based
  std::sort(q.begin(),q.end(),[&](const int& a, const int& b)
    {return (task_list[a].vol_total>task_list[b].vol_total);});

  size_t szTotal=0;

  std::cout << "Allocating arrays..." << std::endl ;
  #pragma omp parallel for schedule(dynamic,1) reduction(+:szTotal) private(i_task)
  for (size_t it=0;it<n_task;it++)
  {
    i_task = q[it];
    if (task_list[i_task].alloc_data()!=0)
    {
      #pragma omp critical
      std::cout << "Failed for i_task=" << i_task << ". Probably out of memory." << std::endl;
    }
    szTotal += task_list[i_task].vol_total;
    #pragma omp simd
    for (size_t i=0;i<task_list[i_task].vol_total;i++)
      ((double*)task_list[i_task].data_src)[i]=i;
  }
  std::cout << "Done! Total size: " << szTotal*sizeof(double)/1024/1024 << " MB" << std::endl ;

  double ts, te;

//
// OpenMP4.5 mapped data_dst
//
  ts=omp_get_wtime();
  #pragma omp parallel for schedule(dynamic,1) private(i_task)
  for (size_t it=0;it<n_task;it++)
  {
    i_task = q[it];
    c_tt_mapped(0,
      task_list[i_task].dim_a,
      task_list[i_task].dim_b,
      task_list[i_task].vol_a,
      task_list[i_task].vol_b,
      task_list[i_task].va2i,
      task_list[i_task].vb2i,
      task_list[i_task].ia2s,
      task_list[i_task].ia2g,
      task_list[i_task].ib2g,
      (const double*)(task_list[i_task].data_src),
      (double*)(task_list[i_task].data_dst));
  }
  #pragma omp taskwait
  te=omp_get_wtime();
  std::cout<<"OpenMP4.5 offload with mapped data_dst:"<<std::endl;
  std::cout<<te-ts<<std::endl;

//  std::cout << "Sanity check...";
//  int d0=30,d1=21,d2=22,d3=25,d4=22,d5=25;
//  int in0  = 2+d0*(2+d1*(2+d2*(2+d3*(2+d4*(0)))));
//  int out0 = 2+d2*(2+d4*(0+d5*(2+d3*(2+d1*(2)))));
//  for(int i=0;i<d5;i++)
//    if (((double*)(task_list[0].data_src))[in0+i*d0*d1*d2*d3*d4]!=
//        ((double*)(task_list[0].data_dst))[out0+i*d2*d4])
//    {
//      std::cout << "Failed!" << std::endl;
//      return -1;
//    }
//  std::cout << "OK!" << std::endl;
}
