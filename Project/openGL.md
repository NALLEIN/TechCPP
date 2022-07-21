## 并行计算的优化技巧
1. shared_memory  && bank conflict 
2. 读取数据的连续性  读取连续的数据速度更快， depthwise conv的对角化操作
3. 增加每个thread的计算量， 增大计算指令/访存指令的比例
4. warp的调度，一个warp不能同时调用所有线程，  防止hang


## GEMM计算
1. Conv操作转换为矩阵乘法

2. depthwise conv 内存读取不连续
对角化

3. SIMD操作 subgroup 