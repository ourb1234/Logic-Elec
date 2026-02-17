#pragma once

#include<vector>
#include<string>
#include<stdexcept>
#include<print>
#include<iostream>

//bit位，便于抽象
class Bit {
private:
	bool value = 0;

public:
	Bit() = default;
	Bit(bool v) :value(v) { }
	bool operator = (bool v) {
		value = v;
		return value;
	}

	operator int() const {
		return value ? 1 : 0;
	}

	Bit operator !() const {
		return !value;
	}

	Bit operator &(const Bit& bit)const {
		return value & bit.value;
	}


	Bit operator |(const Bit& bit)const {
		return value | bit.value;
	}
};

//电路单元
class Unit {
	friend class circuit;
protected:
	std::vector<Bit*> Inputs;
	std::vector<Bit*> Outputs;
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
		for (auto& p : Inputs) p = new Bit();

		Outputs.resize(outputCount);
		for (auto& p : Outputs) p = new Bit();
	}

	//必须实现的函数，执行单元的逻辑
	virtual void Do() {}

	Bit& Input(size_t index) {
		if (index >= Inputs.size()) {
			throw std::out_of_range("Input index out of range");
		}
		if (!Inputs[index]) {
			throw std::runtime_error("Null pointer in Inputs");
		}
		return *Inputs[index];
	}

	Bit& Output(size_t index) {
		if (index >= Outputs.size()) {
			throw std::out_of_range("Input index out of range");
		}
		if (!Outputs[index]) {
			throw std::runtime_error("Null pointer in Inputs");
		}
		return *Outputs[index];
	}

	void Connect(size_t outputIndex, Unit* other, size_t inputIndex) {
		other->Inputs[inputIndex] = Outputs[outputIndex];
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

//信号源,未实现
class Source : public Unit {
public:

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
		Output(0) = Input(0);
	}
};

class XorGate : public Unit {
public:
	XorGate() :Unit(2, 1) {}
	void Do() override {
		Output(0) = Input(0) & !Input(1) | !Input(0) & Input(1);
	}
};

//线路类，包含多个单元
class circuit {
private:
	std::vector<Unit*> units;
	bool IsSorted = false;
	bool IsInitialized = false;
public:
	std::string name;

	void AddUnit(Unit* unit) {
		units.push_back(unit);
		IsSorted = false;
	}

	//拓扑排序，保证每个单元的依赖都在它之前执行
	void Sort() {
		if (IsSorted)return;
		std::vector<Unit*> sorted;
		std::vector<Unit*> unsorted = units;

		while (!unsorted.empty()) {
			bool progress = false;
			for (auto it = unsorted.begin(); it != unsorted.end();) {
				Unit* unit = *it;
				bool canSort = true;
				for (Unit* req : unit->Requires) {
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
				throw std::runtime_error("Cyclic dependency detected");
			}
		}
		IsSorted = true;
		units = sorted;

	}

	virtual void Init() {}

	void Excute() {
		if (!IsInitialized) {
			Init();
			IsInitialized = !IsInitialized;
		}
		Sort();
		for (auto& unit : units) {
			unit->Do();
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
		std::cout << "adder input " << Input(0) << " " << Input(1) << " " << Input(2) << std::endl;
		Excute();
		std::cout << "adder output " << Output(0) << " " << Output(1) << std::endl;
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