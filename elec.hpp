#pragma once

#include<vector>
#include<string>
#include<stdexcept>
#include<print>
#include<iostream>
#include<chrono>
#include<conio.h>
#include<Windows.h>
#include<thread>
#include<queue>
#include<mutex>
int GetInputNonBlocking() {
	if (_kbhit()) {
		return _getch();
	}
	return -1;
}

//bit位，便于抽象
class Bit {
private:
	bool value = 0;
	bool IsHighImpedance = false;//高阻状态，表示该位没有被任何信号驱动，可以理解为悬空
public:
	Bit() = default;
	Bit(bool v) :value(v) { }
	bool operator = (int v) {
		if (v == -1) {
			IsHighImpedance = true;
			return false;
		}
		value = v;
		return value;
	}

	operator int() const {
		if (IsHighImpedance) return -1;
		return value ? 1 : 0;
	}

	bool isHighZ() const { return IsHighImpedance; }
	bool isOne() const { return !IsHighImpedance && value; }
	bool isZero() const { return !IsHighImpedance && !value; }

	Bit operator !() const {
		if (IsHighImpedance)return *this;
		return !value;
	}

	Bit operator &(const Bit& bit)const {
		if (IsHighImpedance)return bit.value;
		return value & bit.value;
	}


	Bit operator |(const Bit& bit)const {
		if (IsHighImpedance)return bit.value;
		return value | bit.value;
	}
};

//电路单元
class Unit {
	friend class circuit;
protected:
	class Node {
	public:
		std::vector<Bit*> Inputs;
		Bit* Output;

		Node() :Output(new Bit()) {}
		Node(const Node& others) {
			for (auto& input : others.Inputs) {
				Inputs.push_back(input);
			}
			Output = others.Output;
		}
		Bit& Value() {
			if (Inputs.empty()) {
				return *Output;
			}
			if (!Output) {
				throw std::runtime_error("Null pointer in Outputs");
			}
			//这里可以添加一些逻辑来计算输出值，或者直接返回输出指针
			Bit Value = false;
			for (auto& input : Inputs) {
				if (!input) {
					throw std::runtime_error("Null pointer in Inputs");
				}
				if (input->isHighZ()) continue;
				Value = Value | *input;
			}
			*Output = Value;
			return *Output;
		}

		void Connect(Bit* bit) {
			Inputs.push_back(bit);
		}
	};

	std::vector<Node> Inputs;
	std::vector<Node> Outputs;
	std::vector<Unit*> Requires;
	//单元内部连接，用于连接子原件
	void SetInput(size_t InputIndex, Unit* _unit, size_t _InputIndex) {
		_unit->Inputs[_InputIndex] = Inputs[InputIndex];
	}
	//单元内部连接，用于连接子原件
	void SetOutput(size_t OutputIndex, Unit* _unit, size_t _OutputIndex) {
		_unit->Outputs[_OutputIndex] = Outputs[OutputIndex];
	}
public:
	//输入输出数量由构造函数指定
	Unit(size_t inputCount, size_t outputCount) {
		Inputs.resize(inputCount);
		Outputs.resize(outputCount);
	}
	virtual bool isSequential() const { return false; }
	//必须实现的函数，执行单元的逻辑
	virtual void Do() {}
	//为对应位设置输入数据
	Bit& Input(size_t index) {
		if (index >= Inputs.size()) {
			throw std::out_of_range("Input index out of range");
		}
		return Inputs[index].Value();
	}
	//为对应位设置输出数据
	Bit& Output(size_t index) {
		if (index >= Outputs.size()) {
			throw std::out_of_range("Input index out of range");
		}
		return Outputs[index].Value();
	}
	//连接两个单元的输入输出，outputIndex是当前单元的输出索引，inputIndex是另一个单元的输入索引
	void Connect(size_t outputIndex, Unit* other, size_t inputIndex) {
		other->Inputs[inputIndex].Connect(Outputs[outputIndex].Output);
		if (std::find(other->Requires.begin(), other->Requires.end(), this) == other->Requires.end())
			other->Requires.push_back(this);
	}
};

//测量门
class Measure :public Unit {
public:
	std::string name = "";
	Measure() :Unit(1, 1) {}

	void Do() override {
		int value = Input(0);
		std::print("{}Measure: {}\n", name, value);
		Output(0) = Input(0);
	}
};

class Measure8bit :public Unit {
public:
	std::string name = "";
	Measure8bit() :Unit(8, 8) {}

	void Do() override {
		int value = 0;
		for (int i = 0; i < 8; ++i) {
			if (Input(i).isOne()) {
				value |= (1 << i);
			}
		}
		std::print("{}Measure: {}\n", name, value);
		for (size_t i = 0; i < Outputs.size(); ++i) {
			Output(i) = Input(i);
		}
	}
};

class MeasureNbit : public Unit {
public:
	std::string name = "";
	MeasureNbit(int n) :Unit(n, n) {}

	void Do() override {
		int value = 0;
		for (int i = 0; i < Outputs.size(); ++i) {
			if (Input(i).isOne()) {
				value |= (1 << i);
			}
		}
		std::print("{}Measure: {}\n", name, value);
		for (size_t i = 0; i < Outputs.size(); ++i) {
			Output(i) = Input(i);
		}
	}
};

class SignedMeasure8bit : public Unit {
public:
	std::string name = "";
	SignedMeasure8bit() : Unit(8, 8) {}

	void Do() override {
		int value = 0;
		bool hasHighZ = false;

		// 读取每一位，检查高阻并组合数值（无符号）
		for (int i = 0; i < 8; ++i) {
			if (Input(i).isHighZ()) {
				hasHighZ = true;
				break;          // 遇到高阻立即标记并退出循环
			}
			if (Input(i).isOne()) {
				value |= (1 << i);
			}
		}

		if (hasHighZ) {
			std::print("{}Measure (signed): HighZ\n", name);
		}
		else {
			// 将有符号解释：若最高位为1，则为负数
			int signedValue = value;
			if (value & 0x80) {          // 最高位为1
				signedValue = value - 256;   // 等价于补码转原值
			}
			std::print("{}Measure (signed): {}\n", name, signedValue);
		}

		// 输出直通（将输入复制到输出）
		for (size_t i = 0; i < Outputs.size(); ++i) {
			Output(i) = Input(i);
		}
	}
};

class SignedMeasureNbit : public Unit {
public:
	std::string name = "";
	SignedMeasureNbit(int n) : Unit(n, n) {}

	void Do() override {
		int value = 0;
		bool hasHighZ = false;

		// 读取每一位，检查高阻并组合数值（无符号）
		for (int i = 0; i < Outputs.size(); ++i) {
			if (Input(i).isHighZ()) {
				hasHighZ = true;
				break;          // 遇到高阻立即标记并退出循环
			}
			if (Input(i).isOne()) {
				value |= (1 << i);
			}
		}

		if (hasHighZ) {
			std::print("{}Measure (signed): HighZ\n", name);
		}
		else {
			// 将有符号解释：若最高位为1，则为负数
			int signedValue = value;
			if (value & (1 << (Outputs.size() - 1))) {          // 最高位为1
				signedValue = value - (1 << Outputs.size());   // 等价于补码转原值
			}
			std::print("{}Measure (signed): {}\n", name, signedValue);
		}

		// 输出直通（将输入复制到输出）
		for (size_t i = 0; i < Outputs.size(); ++i) {
			Output(i) = Input(i);
		}
	}
};

//信号源
class Clock : public Unit {
public:
	Clock() : Unit(0, 1) {}

	void Do() override {
		Output(0) = !Output(0);
	}
};

//手动输入
class ManualInput : public Unit {
public:
	ManualInput(int n) : Unit(0, n) {}

	void Do() override {
		for (size_t i = 0; i < Outputs.size(); ++i) {
			int value;
			std::cout << "Input value for output " << i << ": ";
			std::cin >> value;
			Output(i) = value;
		}
	}
};

class ManualInput8bitBlock : public Unit {
public:
	ManualInput8bitBlock() : Unit(0, 8) {}

	void Do() override {
		int value;
		std::cout << "Input value for output " << ": ";
		std::cin >> value;
		for (int i = 0; i < 8; ++i)
			Output(i) = (value >> i) & 1;
	}
};

class ManualInputNbitBlock : public Unit {
public:
	std::string Name;
	ManualInputNbitBlock(int n) : Unit(0, n) {}

	void Do() override {
		int value;
		std::cout << Name << " Input value for output " << ": ";
		std::cin >> value;
		for (int i = 0; i < Outputs.size(); ++i)
			Output(i) = (value >> i) & 1;
	}
};

class ManualInput8bitBlockByBit : public Unit {
public:
	ManualInput8bitBlockByBit() : Unit(0, 8) {}

	void Do() override {
		int value;
		std::cout << "Input value for output " << ": ";
		std::cin >> value;
		for (int i = 7; i >= 0; i--) {
			Output(i) = value % 10;
			value /= 10;
		}
	}
};

class ManualInputNbitBlockByBit : public Unit {
public:
	std::string Name;
	ManualInputNbitBlockByBit(int n) : Unit(0, n) {}

	void Do() override {
		int value;
		std::cout << Name << " Input value for output " << ": ";
		std::cin >> value;
		for (int i = Outputs.size() - 1; i >= 0; i--) {
			Output(i) = value % 10;
			value /= 10;
		}
	}
};

class ManualInput8bit : public Unit {
private:
	std::queue<int> inputQueue;   // 存放未处理的输入值
	std::mutex mtx;
	std::thread worker;
	bool running = true;
	bool lastClock = false;

	void inputLoop() {
		while (running) {
			int val;
			std::cin >> val;
			std::lock_guard<std::mutex> lock(mtx);
			inputQueue.push(val);
		}
	}

public:
	ManualInput8bit() : Unit(1, 9) {
		worker = std::thread(&ManualInput8bit::inputLoop, this);
	}

	~ManualInput8bit() {
		running = false;
		if (worker.joinable())
			worker.join();
	}

	void Do() override {
		bool clk = Input(0).isOne();
		if (!lastClock && clk) {       // 上升沿采样
			int currentValue = -1;
			{
				std::lock_guard<std::mutex> lock(mtx);
				if (!inputQueue.empty()) {
					currentValue = inputQueue.front();
					inputQueue.pop();
				}
			}
			if (currentValue != -1) {
				for (int i = 0; i < 8; ++i)
					Output(i) = (currentValue >> i) & 1;
				Output(8) = 1;
			}
			else {
				for (int i = 0; i < 8; ++i)
					Output(i) = -1;
				Output(8) = 0;
			}
		}
		lastClock = clk;
	}
};

//按键输入
class KeyInput8bit : public Unit {
private:
	bool lastClock = false;
public:
	// 输入：时钟 (索引 0)
	// 输出：8位数据 (索引 0-7)，有效标志 (索引 8)
	KeyInput8bit() : Unit(1, 9) {}

	virtual bool isSequential() const override { return true; }

	void Do() override {
		bool clk = Input(0).isOne();
		if (!lastClock && clk) {       // 上升沿检测
			// 清除输出：数据位设为高阻，有效标志置 0
			for (int i = 0; i < 8; ++i) {
				Output(i) = -1;        // 高阻
			}
			Output(8) = 0;              // 有效标志清零

			int16_t value = GetInputNonBlocking();
			if (value != -1) {
				for (int i = 0; i < 8; ++i) {
					Output(i) = (value >> i) & 1;
				}
				Output(8) = 1;          // 有效标志置 1
			}
		}
		lastClock = clk;
	}
};

class ASCIIToDigit : public Unit {
public:
	ASCIIToDigit() : Unit(8, 4) {} // 8 位 ASCII 输入，4 位数值输出

	void Do() override {
		// 读取 ASCII 码
		int ascii = 0;
		for (int i = 0; i < 8; ++i) {
			if (Input(i).isOne()) ascii |= (1 << i);
		}

		// 转换为数值
		if (ascii >= '0' && ascii <= '9') {
			int digit = ascii - '0';
			for (int i = 0; i < 4; ++i) {
				Output(i) = (digit >> i) & 1;
			}
		}
		else {
			// 非数字输入，输出高阻或保持
			for (int i = 0; i < 4; ++i) Output(i) = -1;
		}
	}
};

class PullUp : public Unit {
public:
	PullUp() : Unit(0, 1) {}          // 无输入，一个输出
	void Do() override {
		Output(0) = 1;                 // 固定输出 1（可改为 0 作为下拉）
	}
};

//同一个单元内可以直接使用Bit类运算来简化
class AndGate : public Unit {
public:
	AndGate() :Unit(2, 1) {}

	void Do() override {
		Output(0) = Input(0) & Input(1);
	}
};

class OrGate : public Unit {
public:
	OrGate() :Unit(2, 1) {}

	void Do()override {
		Output(0) = Input(0) | Input(1);
	}
};

class NotGate : public Unit {
public:

	NotGate() :Unit(1, 1) {}

	void Do() override {
		Output(0) = !Input(0);
	}
};

class XorGate : public Unit {
public:
	XorGate() :Unit(2, 1) {}
	void Do() override {
		Output(0) = Input(0) & !Input(1) | !Input(0) & Input(1);
	}
};
//8位逻辑门
class AndGate8bit : public Unit {
public:
	AndGate8bit() :Unit(16, 8) {}

	void Do() override {
		for (int i = 0; i < 8; ++i) {
			Output(i) = Input(i) & Input(i + 8);
		}
	}
};

//多输入与
class AndGate8bit_nInput : public Unit {
public:
	AndGate8bit_nInput(int n) :Unit(8 * n, 8) {}

	void Do() override {
		for (int i = 0; i < 8; ++i) {
			Bit result = 1;
			for (int j = 0; j < Inputs.size() / 8; ++j) {
				result = result & Input(i + j * 8);
			}
			Output(i) = result;
		}

	}
};

class OrGate8bit : public Unit {
public:
	OrGate8bit() :Unit(16, 8) {}

	void Do() override {
		for (int i = 0; i < 8; ++i) {
			Output(i) = Input(i) | Input(i + 8);
		}
	}
};

//多输入或
class OrGate8bit_nInput : public Unit {
public:
	OrGate8bit_nInput(int n) :Unit(8 * n, 8) {}

	void Do() override {
		for (int i = 0; i < 8; ++i) {
			Bit result = 0;
			for (int j = 0; j < Inputs.size() / 8; ++j) {
				result = result | Input(i + j * 8);
			}
			Output(i) = result;
		}

	}

};

class NotGate8bit : public Unit {
public:
	NotGate8bit() :Unit(8, 8) {}

	void Do() override {
		for (int i = 0; i < 8; ++i) {
			Output(i) = !Input(i);
		}
	}

};

class XorGate8bit : public Unit {
public:
	XorGate8bit() :Unit(16, 8) {}

	void Do() override {
		for (int i = 0; i < 8; ++i) {
			Output(i) = Input(i) & !Input(i + 8) | !Input(i) & Input(i + 8);
		}
	}
};

//n位与
class AndGateNbit : public Unit {
private:
	int Nbit;
public:
	AndGateNbit(int n) :Unit(n * 2, n), Nbit(n) {}

	void Do()override {
		for (int i = 0; i < Nbit; ++i) {
			Output(i) = Input(i) & Input(i + Nbit);
		}
	}
};

class AndGateNBit_nInput : public Unit {
private:
	int Nbit;
public:
	AndGateNBit_nInput(int n, int inputNum) :Unit(n* inputNum, n), Nbit(n) {}

	void Do() override {
		for (int i = 0; i < Nbit; ++i) {
			Bit result = 1;
			for (int j = 0; j < Inputs.size() / Nbit; ++j) {
				result = result & Input(i + j * Nbit);
			}
			Output(i) = result;
		}

	}
};
//n位或
class OrGateNBit :public Unit {
private:
	int Nbit;
public:
	OrGateNBit(int n) :Unit(n * 2, n), Nbit(n) {}

	void Do()override {
		for (int i = 0; i < Nbit; ++i) {
			Output(i) = Input(i) | Input(i + Nbit);
		}
	}
};

//n位多输入或
class OrGateNBit_nInput : public Unit {
private:
	int Nbit;
public:
	OrGateNBit_nInput(int n, int inputNum) :Unit(n* inputNum, n), Nbit(n) {}

	void Do() override {
		for (int i = 0; i < Nbit; ++i) {
			Bit result = 0;
			for (int j = 0; j < Inputs.size() / Nbit; ++j) {
				result = result | Input(i + j * Nbit);
			}
			Output(i) = result;
		}

	}
};

//n位异或
class XorGateNBit : public Unit {
private:
	int Nbit;
public:
	XorGateNBit(int n) :Unit(n * 2, n), Nbit(n) {}

	void Do()override {
		for (int i = 0; i < Nbit; ++i) {
			Output(i) = Input(i) & !Input(i + Nbit) | !Input(i) & Input(i + Nbit);
		}
	}
};

//n位非
class NotGateNBit : public Unit {
private:
	int Nbit;
public:
	NotGateNBit(int n) :Unit(n, n), Nbit(n) {}

	void Do()override {
		for (int i = 0; i < Nbit; ++i) {
			Output(i) = !Input(i);
		}
	}
};

//特殊单元
//1位存储单元
class StoreUnit : public Unit {
private:
	Bit bit;//存储1位数据
public:
	StoreUnit() : Unit(2, 1) {}// （读写,数据）

	void Do() override {
		if (int(Input(0)) == 1) {
			bit = Input(1);
		}
		Output(0) = bit;
	}
};

//边沿触发的寄存器
class DFlipFlop : public Unit {
	Bit q;
	bool lastClock = false;
public:
	virtual bool isSequential() const { return true; }
	DFlipFlop() : Unit(2, 1) {} // 输入：D, CLK
	void Do() override {
		bool clk = (Input(1) == 1);
		if (!lastClock && clk) {// 上升沿检测
			q = Input(0);// 采样 D
		}
		lastClock = clk;
		if (q == -1) {
			return;//高阻状态不更新输出
		}
		Output(0) = q;// 始终输出 Q
	}
};
//3态门
class TriStateGate : public Unit {
public:
	// 构造函数：2个输入（数据，使能），1个输出
	TriStateGate() : Unit(2, 1) {}

	void Do() override {
		if (int(Input(1)) == 1) {// 使能有效，输出等于输入数据
			Output(0) = Input(0);
		}
		else {// 使能无效，输出为高阻态
			Output(0) = -1;// 用 -1 表示高阻态
		}
	}
};
//多路选择器
class Mux2to4 : public Unit {
public:
	Mux2to4() :Unit(2, 4) {}
	void Do() override {

		//a b -> !a&!b | a&!b | !a&b |a&b

		Output(0) = !Input(0) & !Input(1);
		Output(1) = Input(0) & !Input(1);
		Output(2) = !Input(0) & Input(1);
		Output(3) = Input(0) & Input(1);

	}
};

class Mux3to8 : public Unit {
public:
	Mux3to8() : Unit(3, 8) {}

	void Do() override {
		//a b c -> !a&!b&!c | a&!b&!c | !a&b&!c | a&b&!c | !a&!b&c | a&!b&c | !a&b&c | a&b&c
		Output(0) = !Input(0) & !Input(1) & !Input(2);
		Output(1) = Input(0) & !Input(1) & !Input(2);
		Output(2) = !Input(0) & Input(1) & !Input(2);
		Output(3) = Input(0) & Input(1) & !Input(2);
		Output(4) = !Input(0) & !Input(1) & Input(2);
		Output(5) = Input(0) & !Input(1) & Input(2);
		Output(6) = !Input(0) & Input(1) & Input(2);
		Output(7) = Input(0) & Input(1) & Input(2);
	}
};

//线路类，包含多个单元
class circuit {
private:
	std::vector<Unit*> comboUnits;
	std::vector<Unit*> seqUnits;
	bool IsSorted = false;
	bool IsInitialized = false;
public:
	std::string name;

	circuit& AddUnit(Unit* unit) {
		if (unit->isSequential())
			seqUnits.push_back(unit);
		else
			comboUnits.push_back(unit);
		IsSorted = false;

		return *this;
	}

	//拓扑排序，保证每个单元的依赖都在它之前执行
	void Sort() {
		if (IsSorted) return;

		// 只对组合单元进行拓扑排序
		std::vector<Unit*> sorted;
		std::vector<Unit*> unsorted = comboUnits;

		while (!unsorted.empty()) {
			bool progress = false;
			for (auto it = unsorted.begin(); it != unsorted.end();) {
				Unit* unit = *it;
				bool canSort = true;
				for (Unit* req : unit->Requires) {
					// 如果依赖是时序单元，忽略（因为时序单元的输出是已知的当前值）
					if (req->isSequential()) continue;
					// 否则要求 req 已在 sorted 中
					if (std::find(sorted.begin(), sorted.end(), req) == sorted.end()) {
						canSort = false;
						break;
					}
				}
				if (canSort) {
					sorted.push_back(unit);
					it = unsorted.erase(it);
					progress = true;
				}
				else {
					++it;
				}
			}
			if (!progress) {
				throw std::runtime_error("Cyclic dependency in combinational logic");
			}
		}
		comboUnits = sorted;
		IsSorted = true;
	}

	virtual void Init() {}

	void Excute() {
		if (!IsInitialized) {
			Init();
			IsInitialized = !IsInitialized;
		}
		Sort();
		// 阶段1：计算所有组合单元
		for (Unit* u : comboUnits) {
			u->Do();
		}

		// 阶段2：更新所有时序单元
		for (Unit* u : seqUnits) {
			u->Do();
		}
	}
};

class Mux4to16 :public Unit, public circuit {
public:
	Mux4to16() : Unit(4, 16) {}

	void Init() override {
		Mux3to8* Mux[2];
		for (int i = 0; i < 2; i++) {
			Mux[i] = new Mux3to8;
			AddUnit(Mux[i]);
			for (int j = 1; j < 4; j++) {
				SetInput(j, Mux[i], j);
			}
		}

		AndGate* andGate[18];

		for (int i = 0; i < 2; i++) {
			andGate[i] = new AndGate();
			AddUnit(andGate[i]);

			if (i == 0) {
				NotGate* notGate = new NotGate();
				AddUnit(notGate);
				SetInput(0, notGate, 0);
				notGate->Connect(0, andGate[i], 0);
			}
			else {
				SetInput(0, andGate[i], 0);
			}
		}

		for (int i = 0; i < 16; i++) {
			andGate[i + 2] = new AndGate();
			AddUnit(andGate[i + 2]);
			andGate[i / 8]->Connect(0, andGate[i + 2], 0);
			Mux[i / 8]->Connect(i % 8, andGate[i + 2], 1);
			SetOutput(i, andGate[i + 2], 0);
		}
	}

	void Do() override {
		Excute();
	}
};
/*
* a b cin -> sum cout
* 0 0 0   -> 0   0
* 1 0 0   -> 1   0
* 0 1 0   -> 1   0
* 0 0 1   -> 1   0
* 1 1 0   -> 0   1
* 1 0 1   -> 0   1
* 0 1 1   -> 0   1
* 1 1 1   -> 1   1
*
* sum = a XOR b XOR cin
* cout = (a AND b) OR (cin AND (a XOR b))
*/
class Adder : public Unit, public circuit {
public:
	Adder() :Unit(3, 2) {}

	void Init() override {
		AndGate* andGate1 = new AndGate();
		AndGate* andGate2 = new AndGate();
		OrGate* orGate = new OrGate();
		XorGate* xorGate1 = new XorGate();
		XorGate* xorGate2 = new XorGate();

		//连接
		//a,b ->xorGate(a,b)
		//cin ->xorGate2(cin,xorgate))
		SetInput(0, xorGate1, 0);
		SetInput(1, xorGate1, 1);
		SetInput(2, xorGate2, 0);
		xorGate1->Connect(0, xorGate2, 1);
		//sum = xorgate2 -> out

		//a,b -> and1
		//cin,a Xor b (xorGate1) -> And2
		//and1 or and2
		SetInput(0, andGate1, 0);
		SetInput(1, andGate1, 1);
		SetInput(2, andGate2, 0);
		xorGate1->Connect(0, andGate2, 1);
		andGate1->Connect(0, orGate, 0);
		andGate2->Connect(0, orGate, 1);

		SetOutput(0, xorGate2, 0);
		SetOutput(1, orGate, 0);
		AddUnit(andGate1);
		AddUnit(andGate2);
		AddUnit(orGate);
		AddUnit(xorGate1);
		AddUnit(xorGate2);

	}

	void Do() override {
		Excute();
	}
};

//8位3态门
class TriStateGate8bit : public Unit, public circuit {
public:
	//8位数据输入 1位使能 -> 8位数据输出
	TriStateGate8bit() :Unit(9, 8) {}

	void Init() override {
		TriStateGate* gates[8];
		for (int i = 0; i < 8; i++) {
			gates[i] = new TriStateGate();
			SetInput(i, gates[i], 0);//数据输入
			SetInput(8, gates[i], 1);//使能输入
			SetOutput(i, gates[i], 0);//数据输出
			AddUnit(gates[i]);
		}
	}

	void Do() override {
		Excute();
	}
};

//8位寄存器
class Rigster : public Unit, public circuit {
public:
	//8位输入，1位时钟，1位写使能,1位读使能-> 8位输出
	Rigster() :Unit(11, 8) {}
	virtual bool isSequential() const { return true; }
	void Init() override {
		DFlipFlop* dffs[8];
		TriStateGate8bit* WriteEnable = new TriStateGate8bit();
		TriStateGate8bit* ReadEnable = new TriStateGate8bit();
		SetInput(10, ReadEnable, 8);//连接读使能到三态门使能输入
		SetInput(9, WriteEnable, 8);//连接写使能到三态门使能输入
		for (int i = 0; i < 8; i++) {
			dffs[i] = new DFlipFlop();
			SetInput(i, WriteEnable, 0);//数据输入
			WriteEnable->Connect(i, dffs[i], 0);//连接写数据到寄存器输入 
			SetInput(8, dffs[i], 1);//时钟输入
			dffs[i]->Connect(0, ReadEnable, i);//连接寄存器输出到三态门输入
			SetOutput(i, ReadEnable, i);//数据输出
			AddUnit(dffs[i]);
		}
	}

	void Do() override {
		Excute();
	}
};

using MemoryUnit = Rigster;//内存单元，和寄存器功能一样，只是名字不同，便于理解

//内存块，包含多个内存单元，可以通过地址输入选择读写哪个单元
//一个内存块8个单元
class MemoryBlock : public Unit, public circuit {
public:
	//输入：8位数据输入,1位时钟,1位写控制，1位读控制，3位地址输入
	//共8个内存单元，3位地址可以映射到8个单元
	//输出：8位数据输出
	MemoryBlock() :Unit(14, 8) {}
	virtual bool isSequential() const { return true; }

	void Init() override {
		MemoryUnit* memUnits[8];
		Mux3to8* mux = new Mux3to8();//地址选择器
		SetInput(11, mux, 0);
		SetInput(12, mux, 1);
		SetInput(13, mux, 2);
		for (int i = 0; i < 8; i++) {
			memUnits[i] = new MemoryUnit();

			for (int j = 0; j < 8; j++) {
				SetInput(j, memUnits[i], j);//数据输入
				SetOutput(j, memUnits[i], j);//数据输出
			}
			//确保选择对应的内存地址
			mux->Connect(i, memUnits[i], 9);//连接地址选择到每个内存单元的写使能输入
			mux->Connect(i, memUnits[i], 10);//连接地址选择到每个内存单元的读使能输入

			//读写使能输入
			SetInput(9, memUnits[i], 9);//写控制输入
			SetInput(10, memUnits[i], 10);//读控制输入

			SetInput(8, memUnits[i], 8);//时钟输入
			AddUnit(memUnits[i]);
		}
	}

	void Do() override {
		Excute();
	}
};

class Adder8bit : public Unit, public circuit {
public:
	//8bit 加数 8bit被加数 1位进位 -> 8bit和 1位进位 5个标记位（占位）
	Adder8bit() :Unit(17, 14) {}

	void Init() override {
		Adder* adderTemp = nullptr;
		for (int i = 0; i < 8; i++) {
			Adder* adder = new Adder();
			if (i == 0) {
				SetInput(16, adder, 2);
			}
			else {
				adderTemp->Connect(1, adder, 2);//连接前一个加法器的进位输出到当前加法器的进位输入
			}

			SetInput(i, adder, 0);//绑定1位输入
			SetInput(i + 8, adder, 1);//绑定1位输入

			SetOutput(i, adder, 0);//绑定1位输出
			adderTemp = adder;
			AddUnit(adder);
		}
		SetOutput(8, adderTemp, 1);//绑定最后一个加法器的进位输出到整体的进位输出
	}

	void Do() override {
		Excute();
	}
};

class AdderNbit : public Unit, public circuit {
private:
	int Nbit;
public:
	//输入：Nbit加数 Nbit被加数 1位进位 -> Nbit和 1位进位 5个标记位（占位）
	AdderNbit(int n) : Unit(2 * n + 1, n + 6), Nbit(n) {}

	void Init() override {
		std::vector<Adder*> adders(Nbit);
		for (int i = 0; i < Nbit; i++) {
			adders[i] = new Adder();
			if (i == 0) {
				SetInput(2 * Nbit, adders[i], 2);
			}
			else {
				adders[i - 1]->Connect(1, adders[i], 2);//连接前一个加法器的进位输出到当前加法器的进位输入
			}

			SetInput(i, adders[i], 0);//绑定1位输入
			SetInput(i + Nbit, adders[i], 1);//绑定1位输入

			SetOutput(i, adders[i], 0);//绑定1位输出
			AddUnit(adders[i]);
		}
		SetOutput(Nbit, adders[Nbit - 1], 1);//绑定最后一个加法器的进位输出到整体的进位输出
	}

	void Do() override {
		Excute();
	}
};

class ALU8bit :public Unit, public circuit {
public:
	//8bitALU单元
	//8bit 输入A 8bit输入B 1bit进位信息 3bit操作码(最多8个操作，加减，与或非)
	//8bit输出结果 1bit进位输出 , 5bit标记位（占位）
	ALU8bit() :Unit(20, 14) {}

	void Init() override {
		//8bit加法器
		Adder8bit* adder = new Adder8bit();
		AddUnit(adder);
		//3-8解码器
		//加减乘除，与或，左移右移
		Mux3to8* decoder = new Mux3to8();
		AddUnit(decoder);
		//输入与门
		OrGate8bit_nInput* OutputGate = new OrGate8bit_nInput(8);//8输入或门，连接8个操作的输出到结果输出
		AddUnit(OutputGate);
		for (size_t index = 0; index < 8; index++) {
			SetOutput(index, OutputGate, index);//连接结果输出到8输入与门的输出
		}


		//连接操作码到解码器
		for (size_t index = 0; index < 3; index++) {
			SetInput(17 + index, decoder, index);
		}

		{
			//加法器处理
			//输入A ,加减相同
			for (size_t index = 0; index < 8; index++) {
				SetInput(index, adder, index);//按位连接输入A
			}

			//输入B,加法直接输入，减法取反输入，进位输入1
			//3个8位与门来实现输入的选择
			//1个8位非门来实现B取反
			AndGate8bit* andGateAdd[2];
			for (size_t index = 0; index < 2; index++) {
				andGateAdd[index] = new AndGate8bit();
				AddUnit(andGateAdd[index]);
			}
			NotGate8bit* notGate = new NotGate8bit();
			AddUnit(notGate);
			//1位非处理减法输入和进位输入
			NotGate* SubNotGate = new NotGate();
			AddUnit(SubNotGate);
			decoder->Connect(1, SubNotGate, 0);//连接操作码的第2位到减法非门输入
			//连接B输入
			for (size_t index = 0; index < 8; index++) {
				SetInput(index + 8, andGateAdd[0], index);//按位连接输入B到第一个与门
				//连接解码器信号，减法信号对应第二个信号
				//第一个门对应加法，用非门连接
				SubNotGate->Connect(0, andGateAdd[0], index + 8);
				SetInput(index + 8, notGate, index);//按位连接输入B到非门
				notGate->Connect(index, andGateAdd[1], index);//连接非门输出到第二个与门
				decoder->Connect(1, andGateAdd[1], index + 8);//连接操作码的第2位到第二个与门输入
			}

			//1个或门来选择加法器输入，连接两个与门输出到加法器输入
			OrGate8bit* orGate8bit = new OrGate8bit();
			AddUnit(orGate8bit);

			//连接两个与门输入到加法器输入
			for (size_t index = 0; index < 8; index++) {
				andGateAdd[0]->Connect(index, orGate8bit, index);
				andGateAdd[1]->Connect(index, orGate8bit, index + 8);
				orGate8bit->Connect(index, adder, index + 8);
			}

			//连接进位信息
			//如果是减法，进位输入位1，使用1位或实现 
			OrGate* orGate = new OrGate();
			AddUnit(orGate);
			decoder->Connect(1, orGate, 0);
			SetInput(16, orGate, 1);
			orGate->Connect(0, adder, 16);//连接或门输出到加法器的进位输入

			//连接加减法到输出门
			for (size_t i = 0; i < 2; i++) {
				AndGate8bit* andGateOp = new AndGate8bit();
				AddUnit(andGateOp);
				for (size_t index = 0; index < 8; index++) {
					adder->Connect(index, andGateOp, index);
					andGateOp->Connect(index, OutputGate, index + i * 8);
					decoder->Connect(i, andGateOp, index + 8);
				}
			}
		}

		//与或实现
		//8位与门
		{
			AndGate8bit* AndGate = new AndGate8bit();
			AddUnit(AndGate);
			AndGate8bit* andGateOp = new AndGate8bit();
			AddUnit(andGateOp);
			for (size_t index = 0; index < 8; index++) {
				SetInput(index, AndGate, index);
				SetInput(index + 8, AndGate, index + 8);
				AndGate->Connect(index, andGateOp, index);
				decoder->Connect(2, andGateOp, index + 8);
				andGateOp->Connect(index, OutputGate, index + 16);
			}
		}

		//8位或门
		{
			OrGate8bit* OrGate = new OrGate8bit();
			AddUnit(OrGate);
			AndGate8bit* andGateOp = new AndGate8bit();
			AddUnit(andGateOp);
			for (size_t index = 0; index < 8; index++) {
				SetInput(index, OrGate, index);
				SetInput(index + 8, OrGate, index + 8);
				OrGate->Connect(index, andGateOp, index);
				decoder->Connect(3, andGateOp, index + 8);
				andGateOp->Connect(index, OutputGate, index + 24);
			}

		}

		//8位非
		{
			NotGate8bit* notGate = new NotGate8bit();
			AddUnit(notGate);
			AndGate8bit* andGateOp = new AndGate8bit();
			AddUnit(andGateOp);
			for (size_t index = 0; index < 8; index++) {
				SetInput(index, notGate, index);
				notGate->Connect(index, andGateOp, index);
				decoder->Connect(4, andGateOp, index + 8);
				andGateOp->Connect(index, OutputGate, index + 32);
			}
			//非操作只使用输入A，输入B不连接
		}
	}

	void Do() override {
		Excute();
	}
};

class ALU : public Unit, public circuit {
private:
	int Nbit;
public:
	//2*n输入 1bit进位信息 3bit操作码(最多8个操作，加减，与或非) -> n bit输出 1bit进位输出 , 5bit标记位（占位）
	ALU(int n) : Unit(2 * n + 1 + 3, n + 6), Nbit(n) {}
	void Init() override {
		//位数匹配的加法器
		AdderNbit* adder = new AdderNbit(Nbit);
		SetOutput(Nbit, adder, Nbit);
		AddUnit(adder);
		//3-8解码器
		//加减乘除，与或，左移右移
		Mux3to8* decoder = new Mux3to8();
		AddUnit(decoder);
		//输入或门
		OrGateNBit_nInput* OutputGate = new OrGateNBit_nInput(Nbit, 8);//8输入或门，连接8个操作的输出到结果输出
		AddUnit(OutputGate);
		for (size_t index = 0; index < Nbit; index++) {
			SetOutput(index, OutputGate, index);//连接结果输出到8输入与门的输出
		}


		//连接操作码到解码器
		for (size_t index = 0; index < 3; index++) {
			SetInput(2 * Nbit + 1 + index, decoder, index);
		}

		{
			//加法器处理
			//输入A ,加减相同
			for (size_t index = 0; index < Nbit; index++) {
				SetInput(index, adder, index);//按位连接输入A
			}

			//输入B,加法直接输入，减法取反输入，进位输入1
			//3个8位与门来实现输入的选择
			//1个8位非门来实现B取反
			AndGateNbit* andGateAdd[2];
			for (size_t index = 0; index < 2; index++) {
				andGateAdd[index] = new AndGateNbit(Nbit);
				AddUnit(andGateAdd[index]);
			}
			NotGateNBit* notGate = new NotGateNBit(Nbit);
			AddUnit(notGate);
			//1位非处理减法输入和进位输入
			NotGate* SubNotGate = new NotGate();
			AddUnit(SubNotGate);
			decoder->Connect(1, SubNotGate, 0);//连接操作码的第2位到减法非门输入
			//连接B输入
			for (size_t index = 0; index < Nbit; index++) {
				SetInput(index + Nbit, andGateAdd[0], index);//按位连接输入B到第一个与门
				//连接解码器信号，减法信号对应第二个信号
				//第一个门对应加法，用非门连接
				SubNotGate->Connect(0, andGateAdd[0], index + Nbit);
				SetInput(index + Nbit, notGate, index);//按位连接输入B到非门
				notGate->Connect(index, andGateAdd[1], index);//连接非门输出到第二个与门
				decoder->Connect(1, andGateAdd[1], index + Nbit);//连接操作码的第2位到第二个与门输入
			}

			//1个或门来选择加法器输入，连接两个与门输出到加法器输入
			OrGateNBit* orGateNbit = new OrGateNBit(Nbit);
			AddUnit(orGateNbit);

			//连接两个与门输入到加法器输入
			for (size_t index = 0; index < Nbit; index++) {
				andGateAdd[0]->Connect(index, orGateNbit, index);
				andGateAdd[1]->Connect(index, orGateNbit, index + Nbit);
				orGateNbit->Connect(index, adder, index + Nbit);
			}

			//连接进位信息
			//如果是减法，进位输入位1，使用1位或实现 
			OrGate* orGate = new OrGate();
			AddUnit(orGate);
			decoder->Connect(1, orGate, 0);
			SetInput(2 * Nbit, orGate, 1);
			orGate->Connect(0, adder, 2 * Nbit);//连接或门输出到加法器的进位输入

			//连接加减法到输出门
			for (size_t i = 0; i < 2; i++) {
				AndGateNbit* andGateOp = new AndGateNbit(Nbit);
				AddUnit(andGateOp);
				for (size_t index = 0; index < Nbit; index++) {
					adder->Connect(index, andGateOp, index);
					andGateOp->Connect(index, OutputGate, index + i * Nbit);
					decoder->Connect(i, andGateOp, index + Nbit);
				}
			}
		}

		//与或实现
		//8位与门
		{
			AndGateNbit* AndGate = new AndGateNbit(Nbit);
			AddUnit(AndGate);
			AndGateNbit* andGateOp = new AndGateNbit(Nbit);
			AddUnit(andGateOp);
			for (size_t index = 0; index < Nbit; index++) {
				SetInput(index, AndGate, index);
				SetInput(index + Nbit, AndGate, index + Nbit);
				AndGate->Connect(index, andGateOp, index);
				decoder->Connect(2, andGateOp, index + Nbit);
				andGateOp->Connect(index, OutputGate, index + 2 * Nbit);
			}
		}

		//8位或门
		{
			OrGateNBit* OrGate = new OrGateNBit(Nbit);
			AddUnit(OrGate);
			AndGateNbit* andGateOp = new AndGateNbit(Nbit);
			AddUnit(andGateOp);
			for (size_t index = 0; index < Nbit; index++) {
				SetInput(index, OrGate, index);
				SetInput(index + Nbit, OrGate, index + Nbit);
				OrGate->Connect(index, andGateOp, index);
				decoder->Connect(3, andGateOp, index + Nbit);
				andGateOp->Connect(index, OutputGate, index + 3 * Nbit);
			}

		}

		//8位非
		{
			NotGateNBit* notGate = new NotGateNBit(Nbit);
			AddUnit(notGate);
			AndGateNbit* andGateOp = new AndGateNbit(Nbit);
			AddUnit(andGateOp);
			for (size_t index = 0; index < Nbit; index++) {
				SetInput(index, notGate, index);
				notGate->Connect(index, andGateOp, index);
				decoder->Connect(4, andGateOp, index + Nbit);
				andGateOp->Connect(index, OutputGate, index + 4 * Nbit);
			}
			//非操作只使用输入A，输入B不连接
		}
	}

	void Do() override {
		Excute();
	}
};
//一个8位总线单元
class Bus8bit : public Unit {
public:
	Bus8bit() :Unit(8, 8) {}

	void Do() override {
		for (int i = 0; i < 8; ++i) {
			Output(i) = Input(i);
		}
	}
};

class CPU {
public:
	CPU() {

	}
};