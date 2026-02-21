#include"elec.hpp"

int main() {
	ManualInputNbitBlockByBit* input = new ManualInputNbitBlockByBit(16);
	input->Name = "Op";
	circuit* c = new circuit();
	ManualInputNbitBlock* inputA = new ManualInputNbitBlock(16);
	inputA->Name = "A";
	ManualInputNbitBlock* inputB = new ManualInputNbitBlock(16);
	inputB->Name = "B";
	ALU* alu = new ALU(16);

	for (size_t index = 0; index < 16; index++) {
		inputA->Connect(index, alu, index);
		inputB->Connect(index, alu, index + 16);
	}

	for (size_t index = 0; index < 3; index++) {
		input->Connect(index + 13, alu, index + 33);
	}

	SignedMeasureNbit* measure = new SignedMeasureNbit(16);
	for (size_t index = 0; index < 16; index++) {
		alu->Connect(index, measure, index);
	}

	c->AddUnit(inputA).AddUnit(inputB).AddUnit(input)
		.AddUnit(alu).AddUnit(measure);

	while (1) {
		c->Excute();
	}
}