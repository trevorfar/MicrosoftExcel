#include "interface.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>
#define TABLE_SIZE 70

//Structure of a cell, I am creating a new type: CellContent that I can assign to things to
//access if its a formula or not, and assign a value

typedef struct {
    int isFormula; //1 is formula 0 isnt
    int hasDependents;
    union {
        double numericValue; // if text is number, its stored here
        char textValue[20]; // else if text is a string/char its stored here
    };
    char originalText[20];
    char formula[20];
} CellContent;

typedef struct DependentCells {
    // Add any fields needed to represent dependent cells
    struct CellEntry *dependentCell;
    struct DependentCells *forward;
} DependentCells;

typedef struct CellEntry{
    ROW row;
    COL col;
    CellContent content;
    struct CellEntry *next; // separate chaining ( linked list )
    DependentCells *forward;
}CellEntry;

typedef struct{
    CellEntry *table[TABLE_SIZE];
}hashTable;

hashTable spreadsheet_table;

int hash(ROW row, COL col){
    char key[10];

    snprintf(key, sizeof(key), "%d%d", row, col); //

    int hashValue = 0;
    for (int i =0; i < strlen(key); i++){
        hashValue += key[i];
    }
    return hashValue % TABLE_SIZE;
}

int isFormula(const char *text){
    return (text[0] == '='); // formula checker, returns 1 if true 0 if false
}


double getCellValue(ROW row, COL col) {
    int foundIndex = hash(row, col);
    CellEntry *current = spreadsheet_table.table[foundIndex];

    while (current != NULL) {
        if (current->row == row && current->col == col) {
            return current->content.numericValue;
        }
        current = current->next;
    }

    return 0;
}


void addDependent(CellEntry *cell, CellEntry *dependentCell) { // Sets the first arg to the second args linked list
    // this linked list represents all cells that depend on it
    // Create a new DependentCells node
    DependentCells *newDependency = (DependentCells *)malloc(sizeof(DependentCells));
    if (newDependency == NULL) {
        exit(3);
    }

    // Set the dependent cell in the new node
    newDependency->dependentCell = cell;

    // Add the new node to the front of the dependency list
    newDependency->forward = dependentCell->forward;
    dependentCell->forward = newDependency;
    dependentCell->content.hasDependents = 1;
}


CellEntry *findCell(ROW row, COL col) {
    int foundIndex = hash(row, col);
    CellEntry *current = spreadsheet_table.table[foundIndex];

    while (current != NULL) {
        if (current->row == row && current->col == col) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}


double evaluateFormula(const char *text, CellEntry *current) {
    double result = 0.0;
    char operator = '+';

    // Go through and evaluate the expression
    while (*text != '\0') {
        // Case 1: no cell dependencies, just integers
        if (isdigit(*text)) {
            double operand = strtod(text, (char**)&text);
            switch(operator) {
                case '+':
                    result += operand;
                    break;
                case '-':
                    result -= operand;
                    break;
                case '/':
                    result /= operand;
                    break;
                case '*':
                    result *= operand;
                    break;
                case '%':
                    result = fmod(result, operand);
                    break;
            }
        } else if (isalpha(*text)) { // Check if it's a column (letter)
            COL col = *text - 'A';
            text++;

            // Extract the row from the cell reference
            ROW row;
            char *endptr;

            row = strtol(text, &endptr, 10) - 1;

            // Check if strtol was successful
            if (endptr != text) {
                // Update the text pointer to the position after the extracted number
                text = endptr;
            } else {
                // Handle the case where strtol failed to extract a valid number
                // For simplicity, setting result to NaN
                result = NAN;
                break;
            }

            // Lookup the cell value for the referenced cell
            double cellValue = getCellValue(row, col);
            if (isnan(cellValue)) {
                // Handle non-numeric dependency
                result = NAN;
                break;
            }

            // Recursively evaluate dependencies
            double depValue = cellValue;
            addDependent(current, findCell(row, col));
            // Update the result based on the operator
            switch (operator) {
                case '+':
                    result += depValue;
                    break;
                case '-':
                    result -= depValue;
                    break;
                case '*':
                    result *= depValue;
                    break;
                case '/':
                    result /= depValue;
                    break;
                case '%':
                    result = fmod(result, depValue);
                    break;
            }

            // Skip to the next operator or the end of the text
            while (*text != '\0' && !strchr("+-*/%", *text)) {
                text++;
            }
            operator = *text;
            text++;
        } else if (strchr("+-/*%", *text)) {
            operator = *text;
            text++;
        } else {
            text++;
        }
    }

    return result;
}




void updateDependents(CellEntry *cell){
    if (cell == NULL){
        return;
    }
    DependentCells *currentCellDependency = cell->forward;
    while (currentCellDependency != NULL) {
        ROW depRow = currentCellDependency->dependentCell->row;
        COL depCol = currentCellDependency->dependentCell->col;
        currentCellDependency = currentCellDependency->forward;
    }

}

void updateDependencies(){
    for (int i = 0; i < TABLE_SIZE; i++){
        CellEntry *current = spreadsheet_table.table[i];
        while (current != NULL) {
            if (current->content.hasDependents) {
                updateDependents(current);
            }
            current = current->next;
        }
    }
}




CellEntry *createHashNode(int index, ROW row, COL col, char *text) {
    CellEntry *newNode = (CellEntry *)malloc(sizeof(CellEntry));
    if (newNode == NULL) {
        return NULL;
    }

    newNode->row = row;
    newNode->col = col;
    newNode->next = spreadsheet_table.table[index];
    newNode->forward = NULL;

    if (isFormula(text)) {

        newNode->content.isFormula = true;
        newNode->content.hasDependents = 0; // Initialize hasDependents to 0

        strcpy(newNode->content.originalText, text);
        double numericValue = evaluateFormula(text, newNode);
        newNode->content.numericValue = numericValue;
        strcpy(newNode->content.textValue, "");
    }
    else {

        newNode->content.isFormula = 0;
        newNode->content.hasDependents = 0; // Initialize hasDependents to 0

        char *endptr;
        double numericValue = strtod(text, &endptr);
        if (*endptr == '\0') {
            newNode->content.numericValue = numericValue;
        } else {
            strcpy(newNode->content.textValue, text);
        }
        strcpy(newNode->content.originalText, text);
    }

    CellEntry *temp = spreadsheet_table.table[index];
    while (temp != NULL) {
        CellEntry *next = temp->next;
        free(temp);
        temp = next;
    }

    spreadsheet_table.table[index] = newNode;
    update_cell_display(row, col, text);
    return newNode;
}




void model_init() {
    // TODO: implement this.
    for (int i =0; i < TABLE_SIZE; i++){
        spreadsheet_table.table[i] = NULL;
    }
}

// Function to update or create a cell
void updateOrCreateCell(CellEntry *cell, ROW row, COL col, char *text) {

    if (isFormula(text)) {
        strcpy(cell->content.originalText, text);
        char *formulaText = text + 1;
        double numericValue = evaluateFormula(text, cell); // Skip the '=' sign for formulas


        if (!isnan(numericValue)) {
            cell->content.numericValue = numericValue;
            char numericValueStr[20]; // Assuming a maximum of 20 characters
            snprintf(numericValueStr, sizeof(numericValueStr), "%.2f", cell->content.numericValue);
            update_cell_display(row, col, numericValueStr);
        } else {
            strcpy(cell->content.textValue, text + 1);
            update_cell_display(row, col, cell->content.textValue);
        }
        strcpy(cell->content.formula, text);
    } else {
        char *txtPointer;
        double numberValue = strtod(text, &txtPointer);

        if (*txtPointer != '\0') {
            strcpy(cell->content.textValue, text);
            update_cell_display(row, col, cell->content.textValue);
        } else {
            cell->content.numericValue = numberValue;
            char display[20];
            snprintf(display, sizeof(display), "%.2f", numberValue);
            update_cell_display(row, col, display);
        }
    }
    updateDependencies();
}


void set_cell_value(ROW row, COL col, char *text) {
    // Find the desired index in my hashtable to store the current value
    int foundIndex = hash(row, col);
    CellEntry *current = spreadsheet_table.table[foundIndex];

    // Loop to the end of my hash table
    while (current != NULL) {
        if (current->row == row && current->col == col) { // Already exists, add into this index
            updateOrCreateCell(current, row, col, text);
            return; // Found and updated the existing cell, exit the function
        }
        current = current->forward;
    }

    // DNE, create a new cell
    CellEntry *newNode = createHashNode(foundIndex, row, col, text);
    updateOrCreateCell(newNode, row, col, text);
}


void clear_cell(ROW row, COL col) {
    // TODO: implement this.
    int foundIndex = hash(row, col);

    CellEntry *current = spreadsheet_table.table[foundIndex];
    CellEntry *prev = NULL;
    //want to set the numerical and text to empty

    while (current != NULL){
        if (current->row == row && current->col == col){
            current->content.isFormula = 0;
            current->content.numericValue = 0;
            current->content.textValue[0] = '\0';
            update_cell_display(row, col, "");

            if (prev == NULL) { // Remove from linked list
                spreadsheet_table.table[foundIndex] = current->next;
            } else {
                prev->next = current->next;
            }
            update_cell_display(row, col, "");
            free(current);
            return;

        }
        prev = current;
        current = current->next;

    }

}
char *get_textual_value(ROW row, COL col) {
    // Find the index in the hashtable
    int foundIndex = hash(row, col);
    CellEntry *current = spreadsheet_table.table[foundIndex];

    // Loop through the linked list at the specified index
    while (current != NULL) {
        if (current->row == row && current->col == col) {
            // Check if it's a text value
            if (current->content.textValue[0] != '\0') {// if theres no null character at the end it has to be a text val
                return strdup(current->content.originalText);
            }
            else  {
//                char *result = (char *)malloc(20); //Creates the string for my numeric values
//                if (result == NULL) { // Memory allocation error, shouldnt reach here
//                    return NULL;
//                }
//                free(result);
                char numericValueStr[30]; // Will increase if I need to support larger values
                snprintf(numericValueStr, sizeof(numericValueStr), "%.2f", current->content.numericValue);
//                return strdup(numericValueStr);
                return strdup(current->content.originalText);

            }
        }
        current = current->next;
    }
    return NULL; // Base case, basically, null will be displaying unless im in a cell with a value
}