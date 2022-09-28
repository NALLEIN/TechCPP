
1. MEMC
H.263 支持基于块的运动估计(ME)和运动补偿(MC)运动补偿除了在先进的模式中是在块基础上进行外,一般都在宏块上进行.
当前帧中的每一个宏块跟前面帧中制定的搜索区域中的 16*16  进行比较,然后选择最匹配的宏块区域.
对当前帧中的所有宏块的预测近似的得到预测帧,然后计算得到残差帧,这个残差帧然后用变换编码过程来进行编码.
H264帧内预测允许4*4 16*16的块预测
4×4 模式基于分别预测每个 4 ×4(新标准支持 8*8)亮度块，适合表现图像细节部分。
帧内 16×16 预测将整个 16×16 亮度块进行预测，适合平滑图像区
H.264 的帧间预测与以前标准中定义的主要不同之处在于：
（1）使用了比以前更宽广范围的尺寸（从 16×16 到 4×4）； 
（2）使用了亚像素运动矢量（其中亮度采用了四分之一像素精度 MV）； 
（3）运用多参考帧进行预测。

允许运动矢量有非整数的部分,不在整数位置的象素是利用双线性内插原来的象素得到的
不是每一个宏块都能得到一个好的匹配块,这种情况下匹配块和当前块之间的残差可能和原来的宏块有同样多的能量.对于这种情况就不用运动补偿了.
为了避免因为运动矢量而导致的比特流的增加,它们也要差分编码 用三个相邻宏块的运动矢量(一个左边,一个上边,一个右上)来对当前块的运动矢量进行预测

2. 变换编码,量化,之字扫描,VLC(Variable Length Coding)可变长编码
视频编码中应用最广泛的是离散余弦变换DCT
量化后的 8*8DCT 系数转化为一维的阵列后即进行熵编码
zigzag 扫描很好的反应了 DCT 系数分布的规律同时也很好的将非零系数和连零系数区分开来
霍夫曼编码Huffman就是其中的一种
熵编码：基于上下文自适应的可变长度编码（CAVLC）和基于上下文的二进制自适应的算术编码（CABAC）


H263可选项
不限制运动矢量（UMV）的方式，参考位于图象边界之外的象素。许扩展运动矢量的范围。
基于语法的算术编码（SAC） 。编码器和解码器不用霍夫曼熵编码算法而是用算术编码来产生比特流。SAC 帧内编码能降低 10－20％，帧间编码能降低 1－3％
高级预测模式 每个宏块四个运动矢量宏块中的每个亮度块允许有自己的运动矢量。
PB帧模式
高级帧内编码模式
修正的量化模式 使编码器有更高的控制量化步长灵活性. 允许编码器对亮度和色度分量使用不同的量化参数. 扩大了 DCT 系数的范围
去块滤波器模式去块滤波器是一种可选的块边界滤波器，应用于 I 帧和 P 帧的编码过程中。这个滤波器应用于 8×8 块的边界来减少块效应和提高图象质量