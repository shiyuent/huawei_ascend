#include "kernel_operator.h"
using namespace AscendC;
constexpr int32_t BUFFER_NUM = 2;

class KernelSinh {
public:
    __aicore__ inline KernelSinh() {}
    __aicore__ inline void Init(GM_ADDR x, GM_ADDR y, uint32_t totalLength, uint32_t tileNum)
    {
        //考生补充初始化代码
        ASSERT(GetBlockNum() != 0 && "block dim can not be zero!");
        this->blockLength = totalLength / GetBlockNum();
        this->tileNum = tileNum;
        ASSERT(tileNum != 0 && "tile num can not be zero!");
        this->tileLength = this->blockLength / tileNum / BUFFER_NUM;

        xGm.SetGlobalBuffer((__gm__ half*)x + this->blockLength * GetBlockIdx(), this->blockLength);
        yGm.SetGlobalBuffer((__gm__ half*)y + this->blockLength * GetBlockIdx(), this->blockLength);

        pipe.InitBuffer(inQueueX, BUFFER_NUM, this->tileLength * sizeof(half));
        pipe.InitBuffer(outQueueY, BUFFER_NUM, this->tileLength * sizeof(half));

    }
    __aicore__ inline void Process()
    {
        //考生补充对“loopCount”的定义，注意对Tiling的处理
        int32_t loopCount = this->tileNum * BUFFER_NUM;
        for (int32_t i = 0; i < loopCount; i++) {
            CopyIn(i);
            Compute(i);
            CopyOut(i);
        }
    }

private:
    __aicore__ inline void CopyIn(int32_t progress)
    {
        //考生补充算子代码
        LocalTensor<half> xLocal = inQueueX.AllocTensor<half>();
        DataCopy(xLocal, xGm[progress * this->tileLength], this->tileLength);
        inQueueX.EnQue(xLocal);
    }
    __aicore__ inline void Compute(int32_t progress)
    {
        //考生补充算子计算代码
        LocalTensor<half> xLocal = inQueueX.DeQue<half>();
        LocalTensor<half> yLocal = outQueueY.AllocTensor<half>();
        Exp(xLocal, xLocal, this->tileLength);
	Reciprocal(yLocal, xLocal, this->tileLength);
	Sub(yLocal, xLocal, yLocal, this->tileLength);
        //half scalar = 0.5;
	Muls(yLocal, yLocal, (half) 0.5, this->tileLength);
        outQueueY.EnQue<half>(yLocal);
        inQueueX.FreeTensor(xLocal);

    }
    __aicore__ inline void CopyOut(int32_t progress)
    {
        //考生补充算子代码
        LocalTensor<half> yLocal = outQueueY.DeQue<half>();
        DataCopy(yGm[progress * this->tileLength], yLocal, this->tileLength);
        outQueueY.FreeTensor(yLocal);
    }

private:
    TPipe pipe;
    //create queue for input, in this case depth is equal to buffer num
    TQue<QuePosition::VECIN, BUFFER_NUM> inQueueX;
    //create queue for output, in this case depth is equal to buffer num
    TQue<QuePosition::VECOUT, BUFFER_NUM> outQueueY;
    GlobalTensor<half> xGm;
    GlobalTensor<half> yGm;

    //考生补充自定义成员变量
    uint32_t blockLength;
    uint32_t tileNum;
    uint32_t tileLength;

};

extern "C" __global__ __aicore__ void sinh_custom(GM_ADDR x, GM_ADDR y, GM_ADDR workspace, GM_ADDR tiling) {
    GET_TILING_DATA(tiling_data, tiling);
    KernelSinh op;
    //补充init和process函数调用内容
    op.Init(x, y, tiling_data.totalLength, tiling_data.tileNum);
    op.Process();
}

#ifndef __CCE_KT_TEST__
// call of kernel function
void add_custom_do(uint32_t blockDim, void* l2ctrl, void* stream, uint8_t* x, uint8_t* y, uint8_t* z,
    uint8_t* workspace, uint8_t* tiling)
{
    add_custom<<<blockDim, l2ctrl, stream>>>(x, y, z, workspace, tiling);
}
#endif
