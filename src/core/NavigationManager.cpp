#include "NavigationManager.hpp"
#include <stdexcept>
const QVector<ScreenInfo> &NavigationManager::screens() {
    static const QVector<ScreenInfo> data = {{1, "Splash"},
                                             {2, "Login"},
                                             {3, "Password assistance"},
                                             {4, "Dashboard"},
                                             {5, "User profile"},
                                             {6, "Change password"},
                                             {7, "Customers"},
                                             {8, "Add customer"},
                                             {9, "Customer details"},
                                             {10, "Edit customer"},
                                             {11, "Customer verification / KYC"},
                                             {12, "Customer account history"},
                                             {13, "Bank accounts"},
                                             {14, "Open new account"},
                                             {15, "Account details"},
                                             {16, "Edit account information"},
                                             {17, "Freeze / unfreeze account"},
                                             {18, "Close account"},
                                             {19, "All transactions"},
                                             {20, "Cash deposit"},
                                             {21, "Cash withdrawal"},
                                             {22, "Account transfer"},
                                             {23, "Transaction details"},
                                             {24, "Transaction history & filters"},
                                             {25, "Transaction receipt"},
                                             {26, "Loan applications"},
                                             {27, "Apply for loan"},
                                             {28, "Loan approval / rejection"},
                                             {29, "Loan repayment"},
                                             {30, "Loan details & schedule"},
                                             {31, "Debit card management"},
                                             {32, "Issue / block card"},
                                             {33, "Employee management"},
                                             {34, "Branch management"},
                                             {35, "Financial reports"},
                                             {36, "Customer & account reports"},
                                             {37, "Transaction analytics"},
                                             {38, "User & role management"},
                                             {39, "Audit logs"},
                                             {40, "Settings & database backup"}};
    return data;
}
QString NavigationManager::title(int id) {
    for (const auto &s : screens())
        if (s.id == id)
            return s.title;
    throw std::runtime_error("Unknown screen");
}
void NavigationManager::visit(int id) {
    title(id);
    current_ = id;
}
int NavigationManager::current() const {
    return current_;
}
