## Vulkan优势
Vulkan 采用更接近硬件描述的中间格式 SPIR-V 作为 shader 格式，SPIR-V 作为预
编译过的 binary shader，在运行期的载入速度更快，能够提供更好的应用体验

## arch
BackendWrapper 封装 OpenCV Mat 对象，方便在 CPU 和后端设备之间传递数据，包括但不限于层的输入、输出、权重和偏置。
BackendNode 封装后端计算节点，计算节点指的是层的计算功能。

BackendWrapper 和 BackendNode 对象保存在 LayerData 结构中，LayerData 结构是 DNN 模块操作层对象的关键数据结构，
DNN 模块通过 setUpNet 函数初始化 BackendWrapper 和 BackendNode，通过forwardLayer 函数调用 BackendNode 进行层的前向运算。

```CPP
struct LayerData
{
 /* 这里只列出了更 backend 实现相关的成员变量 */
 std::vector<Ptr<BackendWrapper> > outputBlobsWrappers; // 层输出
 std::vector<Ptr<BackendWrapper> > inputBlobsWrappers; // 层输入
 std::vector<Ptr<BackendWrapper> > internalBlobsWrappers; // 权重和偏置
 std::map<int, Ptr<BackendNode> > backendNodes; // 后端计算节点
 bool skip; // 是否跳过该层。跳过的情况包括该层的运算被合并到别的层中，
 // 或者在使用 Intel 推理引擎后端的情况下，推理运算不再是逐层调用，
 // 而是只在最后一层调用一次推理引擎的内部网络进行前向运算，
}
```

DNN 模块通过 setUpNet 函数初始化 BackendWrapper 和 BackendNode，通过forwardLayer 函数调用 BackendNode 进行层的前向运算

1. BackendWarpper 数据对象初始化
backwend_warper BackendWrapper 封装 OpenCV Mat 对象，方便在 CPU 和后端设备之间传递数据，包括但不限于层的输入、输出、权重和偏置。
allocateLayer()分配每一个具体的层对象，allocateLayer()调用 wrap()初始化后端数据对象（将 CPU 数据对象封装到具体后端的数据对象）。
核心是warp()函数，将Mat对象绑定到后端的BackendWarpper
```CPP
//  为特定的后端和目标设备封装 Mat 对象。
BackendWrapper(int targetId, const cv::Mat& m);
// DNN 模块进行层融合处理的时候，会对内存进行复用; 复用已有的 BackendWrapper 对,但是内存布局需要reshape
BackendWrapper(const Ptr<BackendWrapper>& base, const MatShape& shape);
```

```CPP
class VkComBackendWrapper : public BackendWrapper
{
public:
    VkComBackendWrapper(Mat& m);
    VkComBackendWrapper(const Ptr<BackendWrapper>& baseBuffer, Mat& m);
    virtual void copyToHost() CV_OVERRIDE;
    virtual void setHostDirty() CV_OVERRIDE;
    // 表明目标设备内存数据有更新，需要同步到 CPU
    void setDeviceDirty();
    // CPU 向目标设备拷贝数据
    void copyToDevice();
    // 获取 Vulkan 后端的内部 Tensor 对象
    vkcom::Tensor getTensor();
private:
   vkcom::Tensor tensor;
   // 记录 Mat 对象指针
   Mat* host;
   bool hostDirty;
   bool deviceDirty;
}
```
WGPU Buffer && Tensor
2. BackendNode
BackendNode 封装后端计算节点，计算节点指的是层的计算功能。加速后端需要在层的实现代码中加入初始化代码以创建 BackendNode 对象。

以卷积层的 Vulkan 加速后端为例，convolution_layer.cpp中加入 initVkCom 函数来创建计算节点对象。
layer::initVkcom ->ConvolutionLayerImpl::initVkCom()
VkComBackendNode 
```CPP
initVkComBackend(); Layer::initVkcom();
layer::initVkcom ->ConvolutionLayerImpl::initVkCom()->VkComBackendNode;
// OpBase 将层配置参数传入后端算子的构造函数，创建后端算子
std::shared_ptr<vkcom::OpBase> op(new vkcom::OpConv())

```
3. 算子计算
backendNode::forward
copyToDevice hostDirty
```CPP
bool VkComBackendNode::forward(std::vector<vkcom::Tensor>& outs)
{
 for (int i = 0, n = inputsWrapper_.size(); i < n; ++i)
 {
 // 输入数据同步到后端设备
inputsWrapper_[i].dynamicCast<VkComBackendWrapper>()->copyToDevice();
 }
 // 调用后端算子进行前向运算
 return operation->forward(ins, blobs, outs);
}
```
只有当 hostDirty 标志为真时才会发生数据拷贝。而 hostDirty标志只有在某一层采用 OpenCV 自带的 CPU 实现的时候hostDirty=true

4. Vkcom后端库
- 库维护一个全局唯一的 Vulkan 环境，包括 VkInstance、VkPhysicalDevice、VkDevice、VkQueue、VkCommandPool 类型的对象，
通过 Context 对象进行管理。 初始化 通过 isAvailable()函数确认Vulkan 环境可用并初始化 Vulkan 环境，
- OpBase 是算子抽象类，它集中了每个算子需要用到 vulkan 对象，列如 VkPipeline，VkCommandBuffer，VkShaderModule 对象。
- Tensor 是张量类，它使用 Buffer 类来管理设备内存。Buffer 类则封装了 VkBuffer 和CV::Mat之之间的拷贝和Map
    Buffer tensor类型  buffer asyncread && wait()
- SPIR-V shader 生成  GLSL先写好，然后通过工具 glslangValidator（由 Vulkan SDK 提供）转成 SPIR-V 格式，再被加载到 Vulkan 中运行。
OpBase WGPU计算流程  算子内部只创建一次pipeline可以复用   分组卷积,防止hang

```CPP
    void createBindGroupLayout(int buffer_num);
    void createBindGroup();
    void createShaderModule(const uint32_t* spv,
                            uint32_t size,
                            const std::string& source = std::string());
    void createComputePipeline();
    void createCommandBuffer();   // cv::AutoLock lock(wContextMtx);
    void runCommandBuffer();    //  cv::AutoLock lock(wContextMtx);
```

```CPP
    // __init__ : 
    createBindGroupLayout(4);

    if (pipeline_ == nullptr)
    {
        createShaderModule(softmax_spv, sizeof(softmax_spv)/sizeof(uint32_t));
        createComputePipeline();
        computeGroupCount();
    }

    bindTensor(in,  0, bgEntries);
    bindTensor(*max_tensor_,  1, bgEntries);
    bindTensor(*sum_tensor_,  2, bgEntries);
    bindTensor(out, 3, bgEntries);
    bindUniform(*uniformBuffer_, 4, bgEntries);

    createBindGroup();
    createCommandBuffer();
    runCommandBuffer();
```

Context WGPU device 创建和管理 Device Queue CommandPool等

cv::mutex 上锁管理device


## Vulkan计算 shader


## mutex使用 