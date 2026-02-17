#pragma once

#include<vector>
#include<string>
#include<stdexcept>
#include<print>
#include<iostream>
#include<chrono>

//bit位，便于抽象
class Bit {
private:
	bool value = 0;
	bool IsHighImpedance = false;
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
		if (IsHighImpedance)return *this;
		return value & bit.value;
	}


	Bit operator |(const Bit& bit)const {
		if (IsHighImpedance)return *this;
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

	void SetInput(size_t InputIndex, Unit* _unit, size_t _InputIndex) {
		_unit->Inputs[_InputIndex] = Inputs[InputIndex];
	}

	void SetOutput(size_t OutputIndex, Unit* _unit, size_t _OutputIndex) {
		_unit->Outputs[_OutputIndex] = Outputs[OutputIndex];
	}
public:
	Unit(size_t inputCount, size_t outputCount) {
		Inputs.resize(inputCount);
		Outputs.resize(outputCount);
	}
	virtual bool isSequential() const { return false; }
	//必须实现的函数，执行单元的逻辑
	virtual void Do() {}

	Bit& Input(size_t index) {
		if (index >= Inputs.size()) {
			throw std::out_of_range("Input index out of range");
		}
		return Inputs[index].Value();
	}

	Bit& Output(size_t index) {
		if (index >= Outputs.size()) {
			throw std::out_of_range("Input index out of range");
		}
		return Outputs[index].Value();
	}

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
		if (!lastClock && clk) {       // 上升沿检测
			q = Input(0);               // 采样 D
		}
		lastClock = clk;
		Output(0) = q;                  // 始终输出 Q
	}
};
//3态门
class TriStateGate : public Unit {
public:
	// 构造函数：2个输入（数据，使能），1个输出
	TriStateGate() : Unit(2, 1) {}

	void Do() override {
		if (int(Input(1)) == 1) {       // 使能有效，输出等于输入数据
			Output(0) = Input(0);
		}
		else {                 // 使能无效，输出为高阻态
			Output(0) = -1;      // 用 -1 表示高阻态
		}
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

	void AddUnit(Unit* unit) {
		if (unit->isSequential())
			seqUnits.push_back(unit);
		else
			comboUnits.push_back(unit);
		IsSorted = false;
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

void test() {
	AndGate* andGate = new AndGate();
	Measure* measure = new Measure();
	OrGate* orGate = new OrGate();
	ManualInput* In = new ManualInput(3);

	In->Connect(0, andGate, 0);
	In->Connect(1, andGate, 1);
	In->Connect(2, orGate, 1);

	andGate->Connect(0, orGate, 0);
	orGate->Connect(0, measure, 0);

	circuit c;
	c.AddUnit(andGate);
	c.AddUnit(measure);
	c.AddUnit(orGate);
	c.AddUnit(In);

	c.Excute();
}