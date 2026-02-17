#include"elec.hpp"

int main() {
	Adder* adder = new Adder();

	circuit circuit;
	ManualInput* input = new ManualInput(3);
	input->Connect(0, adder, 0);
	input->Connect(1, adder, 1);
	input->Connect(2, adder, 2);

	Measure* measure = new Measure();
	adder->Connect(0, measure, 0);
	Measure* measure2 = new Measure();
	adder->Connect(1, measure2, 0);

	circuit.AddUnit(adder);
	circuit.AddUnit(input);
	circuit.AddUnit(measure);
	circuit.AddUnit(measure2);

	circuit.Excute();
}