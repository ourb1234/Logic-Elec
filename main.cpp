#include"elec.hpp"

int main() {
	Adder8bit* adder = new Adder8bit();
	adder->Input(16) = 0;
	Measure8bit* measure = new Measure8bit();
	ManualInput8bitBlock* input1 = new ManualInput8bitBlock();
	ManualInput8bitBlock* input2 = new ManualInput8bitBlock();

	for (int i = 0; i < 8; i++) {
		input1->Connect(i, adder, i);
		input2->Connect(i, adder, i + 8);
		adder->Connect(i, measure, i);
	}

	circuit cir;
	cir.AddUnit(input1)
		.AddUnit(input2)
		.AddUnit(adder)
		.AddUnit(measure);
	cir.Excute();
}